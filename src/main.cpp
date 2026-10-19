
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <time.h>                   // for time() ctime()
#include <WebSerial.h>

// Graylog
#include <UDPTransport.h>
#include <Message.h>
#include <Publisher.h>

// Load Cell
#include "HX711.h"

#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h> // For parsing API data
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <AsyncTCP.h>
#include <ElegantOTA.h>
#include <Preferences.h>
#include <LoadSensorStabilizer.h>
#include <PetWeight.h>

#define calibration_factor 9940.0 //This value is obtained using the SparkFun_HX711_Calibration sketch

#define DOUT  32
#define CLK  33

// const int pet_weights_length = 16;

// pet_weight_t pet_weights[pet_weights_length]; // buffer for the data (max 16 entries)

Preferences prefs;

PetWeight pet_weight;

unsigned long ota_progress_millis = 0;

//SSID and Password of your WiFi router
const char* ssid = "enginerdy-iot";
const char* password = "";
String mac_address = "30:76:f5:e7:b7:e0";
char* host_name;

HX711 scale;

// Here we create the transport and the publisher objects
// The transport can be either UDP, TCP, HTTP, etc (right now UDP only is implemented)
// It takes an IP address or a host name

char* graylog_address= "blinking-graylog.pettingzoo.casa";

// UDPTransport transport = UDPTransport("log.mysite.net", 12201);
UDPTransport transport = UDPTransport(graylog_address, 12201);
Publisher publisher = Publisher(&transport);

// We will use this just to avoid creating many messages in our functions
Message message = Message("");

AsyncWebServer server(80);

double current_weight = 0;

/* Configuration of NTP */
#define MY_NTP_SERVER "pool.ntp.org"
#define MY_TZ "EST5EDT,M3.2.0,M11.1.0"

time_t now;                         // this are the seconds since Epoch (1970) - UTC
tm tm;                              // the structure tm holds time information in a more convenient way

unsigned long start_time;

const float steady_weight_tolerance = 0.01;
const int steady_weight_count_required = 20;

int sample_counter = 0;

LoadSensorStabilizer sensor(steady_weight_count_required, steady_weight_tolerance);



int log_to_gelf(const char* short_message) {
    message.reset(short_message);
    message.set("host", host_name);
    message.set("millis", millis());
    publisher.publish(&message);
    return 0;
}


char* get_host_name(){
    String test_mac_string = String("30:76:f5:e7:b7:e0");
    test_mac_string.toLowerCase();
    if( mac_address == test_mac_string) {
        return (char*)"litter_scale_1";
    }

    return (char*)"unknown";
}

void onOTAStart() {
  // Log when OTA has started
  Serial.println("OTA update started!");
  // <Add your own code here>
}

void onOTAProgress(size_t current, size_t final) {
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

void onOTAEnd(bool success) {
  // Log when OTA has finished
  if (success) {
    Serial.println("OTA update finished successfully!");
  } else {
    Serial.println("There was an error during OTA update!");
  }
  // <Add your own code here>
}



void setup() {
    Serial.begin(57600);

    configTzTime(MY_TZ, MY_NTP_SERVER); // --> Here is the IMPORTANT ONE LINER needed in your sketch!

    // Initialize LittleFS
    if(!LittleFS.begin()){
        Serial.println("An Error has occurred while mounting LittleFS");
        return;
    }

    // prefs.begin("pet-scale"); // use "my-app" namespace

    // // prefs.remove("pet_weights");

    // if (not prefs.isKey("pet_weights")) {
    //     // pet_weights[0] = {"Kaylee", 12.5}; // one
    //     prefs.putBytes("pet_weights", pet_weights, sizeof(pet_weights));
    // }

    // size_t pw_pref_len = prefs.getBytesLength("pet_weights");
    // // simple check that data fits
    // if (0 == pw_pref_len || pw_pref_len % sizeof(pet_weight_t) || pw_pref_len > sizeof(pet_weights)) {
    //     Serial.printf("Invalid size of pet_weights array: %zu, should be %zu\n", pw_pref_len, sizeof(pet_weights));
    //     prefs.putBytes("pet_weights", pet_weights, sizeof(pet_weights));
    //     return;
    // }
    // prefs.getBytes("pet_weights", pet_weights, pw_pref_len);


    WiFiManager wm;

    // reset settings - wipe stored credentials for testing
    // these are stored by the esp library
    // wm.resetSettings();

    // Automatically connect using saved credentials,
    // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
    // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
    // then goes into a blocking loop awaiting configuration and will return success result

    bool res;
    // res = wm.autoConnect(); // auto generated AP name from chipid
    // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
    res = wm.autoConnect("Litter-Scale"); // password protected ap

    if(!res) {
        Serial.println("Failed to connect");
        // ESP.restart();
    }
    else {
        //if you get here you have connected to the WiFi    
        Serial.println("connected...yeey :)");
    }


    mac_address = WiFi.macAddress();
    mac_address.toLowerCase();
    Serial.println(mac_address);

    // 1. SERVE REACT FRONTEND STATIC FILES
    // Serves index.html when a user types the ESP32's IP address
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // 2. BACKEND API ENDPOINT (For React to control hardware or fetch stats)
    server.on("/api/status", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument doc;
        doc["temperature"] = 24.5; // Example sensor data
        doc["heap"] = ESP.getFreeHeap();
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    host_name = get_host_name();

    scale.begin(DOUT, CLK);
    scale.set_scale(calibration_factor); //This value is obtained by using the SparkFun_HX711_Calibration sketch
    scale.tare(); //Assuming there is no weight on the scale at start up, reset the scale to 0

    ElegantOTA.begin(&server);    // Start ElegantOTA
    // ElegantOTA callbacks
    ElegantOTA.onStart(onOTAStart);
    ElegantOTA.onProgress(onOTAProgress);
    ElegantOTA.onEnd(onOTAEnd);

    // WebSerial is accessible at "<IP Address>/webserial" in browser
    WebSerial.begin(&server);

    /* Attach Message Callback */
    WebSerial.onMessage([&](uint8_t *data, size_t len) {
        Serial.printf("Received %u bytes from WebSerial: ", len);
        Serial.write(data, len);
        Serial.println();
        WebSerial.println("Received Data...");
        String d = "";
        for(size_t i=0; i < len; i++){
            d += char(data[i]);
        }
        WebSerial.println(d);
    });

    server.begin();

    Serial.println("Start");
    WebSerial.println("Start");

    start_time = millis();
}

void loop() {
    // ArduinoOTA.handle();
    ElegantOTA.loop();
    yield();

    time(&now);                       // read the current time
    localtime_r(&now, &tm);

    current_weight = scale.get_units();
    sensor.addReading(current_weight);
    sample_counter++;

    if( sensor.isSteady() == true ) {
        pet_weight.process_recorded_weight(current_weight);
    }

}



