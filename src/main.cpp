
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <time.h>                   // for time() ctime()

// Graylog
// #include <UDPTransport.h>
// #include <Message.h>
// #include <Publisher.h>

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

#include "mqtt_client.h"

#include <Secrets.h>

// #include <GelfLogger.h>

// #include <Logger.h>

#include <ezButton.h>

#include <ezLED.h> // ezLED library

#include <Syslog.h>


// #define DEFAULT_CALIBRATION_FACTOR 9940.0 //This value is obtained using the SparkFun_HX711_Calibration sketch
#define DEFAULT_CALIBRATION_FACTOR 10980.0 //This value is obtained using the SparkFun_HX711_Calibration sketch

float calibration_factor = DEFAULT_CALIBRATION_FACTOR; //-7050 worked for my 440lb max scale setup

#define DOUT  32
#define CLK  33
#define GREEN_BTN 26
#define YELLOW_BTN 27
#define GREEN_LED 14
#define RED_LED 12

// const int pet_weights_length = 16;

// pet_weight_t pet_weights[pet_weights_length]; // buffer for the data (max 16 entries)

ezButton green_button(GREEN_BTN);
ezButton yellow_button(YELLOW_BTN);

ezLED green_led(GREEN_LED);
ezLED red_led(RED_LED);

Preferences prefs;

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udpClient;

// Create a new empty syslog instance
Syslog syslog(udpClient, SYSLOG_PROTO_BSD);

PetWeight pet_weight(syslog);

unsigned long ota_progress_millis = 0;

//SSID and Password of your WiFi router
const char* ssid = SSID;
const char* password = "";
String mac_address; // = "30:76:f5:e7:b7:e0";
IPAddress ip_address;
char* host_name;

// "Litter-Scale"

HX711 scale;

// Here we create the transport and the publisher objects
// The transport can be either UDP, TCP, HTTP, etc (right now UDP only is implemented)
// It takes an IP address or a host name

const char* graylog_address = GRAYLOG_ADDRESS;

const char* SYSLOG_SERVER = GRAYLOG_ADDRESS;
// #define SYSLOG_SERVER "syslog-server"
const int SYSLOG_PORT = 5140;
// #define SYSLOG_PORT 514



// char* graylog_address_modified = const_cast<char*>(graylog_address);

// UDPTransport transport = UDPTransport("log.mysite.net", 12201);
// UDPTransport transport = UDPTransport(const_cast<char*>(graylog_address), 12201);
// Publisher publisher = Publisher(&transport);

// // We will use this just to avoid creating many messages in our functions
// Message message = Message("");

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

size_t pref_result;

int green_button_last_state = HIGH;
int yellow_button_last_state = HIGH;


PetWeight::ScaleState scale_state, scale_state_new;

// int log_to_gelf(const char* short_message) {
//     Serial.printf("log_to_gelf write Start\n");
//     message.reset(short_message);
//     message.set("host", host_name);
//     message.set("millis", millis());
//     Serial.printf("log_to_gelf write Before Publish host_name: %s, message: %s\n", host_name, short_message);
//     publisher.publish(&message);
//     Serial.printf("log_to_gelf write After Publish\n");
//     Serial.printf("log_to_gelf write Done\n");
//     return 0;
// }


char* get_host_name(){
    String test_mac_string = String(LITTER_SCALE_SMALL_MAC);
    test_mac_string.toLowerCase();
    if( mac_address == test_mac_string) {
        return (char*)"litter_scale_small";
    }

    test_mac_string = String(LITTER_SCALE_LARGE_MAC);
    test_mac_string.toLowerCase();
    if( mac_address == test_mac_string) {
        return (char*)"litter_scale_large";
    }

    return (char*)"unknown";
}

void onOTAStart() {
  // Log when OTA has started
  Serial.println("OTA update started!");
  syslog.log(LOG_INFO, "OTA update started!\n");
  // <Add your own code here>
}

void onOTAProgress(size_t current, size_t final) {
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
    syslog.logf(LOG_INFO, "OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

void onOTAEnd(bool success) {
  // Log when OTA has finished
  if (success) {
    Serial.println("OTA update finished successfully!");
    syslog.log(LOG_NOTICE, "OTA update finished successfully!\n");
  } else {
    Serial.println("There was an error during OTA update!");
    syslog.log(LOG_ERR, "There was an error during OTA update!\n");
  }
  // <Add your own code here>
}



void setup() {
    Serial.begin(115200);
    Serial.println("Serial Started");
    configTzTime(MY_TZ, MY_NTP_SERVER); // --> Here is the IMPORTANT ONE LINER needed in your sketch!

    // Initialize LittleFS
    Serial.println("LittelFS Begin");
    if(!LittleFS.begin()){
        Serial.println("An Error has occurred while mounting LittleFS");
        return;
    }
    Serial.println("Done LittelFS Begin");

    // pinMode(GREEN_BTN, INPUT_PULLUP);
    // pinMode(YELLOW_BTN, INPUT_PULLUP);

    green_button.setDebounceTime(2);
    green_button.setCountMode(COUNT_FALLING);
    yellow_button.setDebounceTime(2);
    yellow_button.setCountMode(COUNT_FALLING);

    green_led.blinkNumberOfTimes(500, 500, 4);
    red_led.blinkNumberOfTimes(500, 500, 3, 500);

    
    prefs.begin("pet-scale"); // use "my-app" namespace

    // // prefs.remove("pet_weights");

    Serial.printf("Check Cal Factor Pref Exists, %d\n", prefs.isKey("cal_factor"));
    if( !prefs.isKey("cal_factor") ) {
        // pet_weights[0] = {"Kaylee", 12.5}; // one
        Serial.println("Set Default Cal Factor Pref");
        prefs.putFloat("cal_factor", DEFAULT_CALIBRATION_FACTOR);
    }
    Serial.println("Done Check Cal Factor Pref Exists");

    Serial.println("Get Cal Factor Pref");
    calibration_factor = prefs.getFloat("cal_factor");
    Serial.printf("Done Get Cal Factor Pref: %f, size: %d\n", calibration_factor, pref_result);

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

    

    mac_address = WiFi.macAddress();
    ip_address = WiFi.localIP();
    mac_address.toLowerCase();
    Serial.printf("MAC Address: %s\n", mac_address.c_str());
    Serial.printf("IP Address: %s\n", ip_address.toString());

    host_name = get_host_name();

    wm.setHostname(host_name);

    Serial.printf("Host Name: %s\n", host_name);

    // GraylogLogger.set_hostname(host_name);

    if(!res) {
        Serial.println("Failed to connect");
        // ESP.restart();
    }
    else {
        //if you get here you have connected to the WiFi    
        Serial.println("connected...yeey :)");
    }


    syslog.server(SYSLOG_SERVER, SYSLOG_PORT);
    syslog.deviceHostname(host_name);
    syslog.appName("Litter-Scale");
    syslog.defaultPriority(LOG_KERN);
    syslog.logMask(LOG_UPTO(LOG_INFO));

    

    // 1. SERVE REACT FRONTEND STATIC FILES
    // Serves index.html when a user types the ESP32's IP address
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // 2. BACKEND API ENDPOINT (For React to control hardware or fetch stats)
    server.on("/api/status", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument doc;
        doc["temperature"] = 24.5; // Example sensor data
        doc["heap"] = ESP.getFreeHeap();
        Serial.printf("head: %u\n", doc["heap"]);
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

   


    scale.begin(DOUT, CLK);
    scale.set_scale(calibration_factor); //This value is obtained by using the SparkFun_HX711_Calibration sketch
    scale.tare(); //Assuming there is no weight on the scale at start up, reset the scale to 0

    ElegantOTA.begin(&server);    // Start ElegantOTA
    // ElegantOTA callbacks
    ElegantOTA.onStart(onOTAStart);
    ElegantOTA.onProgress(onOTAProgress);
    ElegantOTA.onEnd(onOTAEnd);

    server.begin();

    syslog.log(LOG_NOTICE, "Start\n");

    syslog.logf(LOG_NOTICE, "Scale State %s\n", pet_weight.get_state_str());

    start_time = millis();
}

void loop() {
    // ArduinoOTA.handle();
    syslog.logf(LOG_DEBUG, "ElegantOTA.loop\n");
    ElegantOTA.loop();
    yield();
    // syslog.logf(LOG_DEBUG, "green_button.loop\n");
    green_button.loop();
    yield();
    // syslog.logf(LOG_DEBUG, "yellow_button.loop\n");
    yellow_button.loop();
    yield();

    green_led.loop();
    red_led.loop();
    yield();

    if(green_led.getState() == LED_IDLE) {
        green_led.turnON();
    }

    // syslog.logf(LOG_DEBUG, "time\n");
    time(&now);                       // read the current time
    localtime_r(&now, &tm);

    // syslog.logf(LOG_DEBUG, "scale\n");
    current_weight = scale.get_units();
    sensor.addReading(current_weight);
    sample_counter++;

    // syslog.logf(LOG_DEBUG, "steady\n");
    if( sensor.isSteady() == true ) {
        // syslog.logf(LOG_DEBUG, "Steady Weight: %.3f lbs", current_weight);
        // syslog.logf(LOG_DEBUG, "process\n");
        pet_weight.process_recorded_weight(current_weight);
    }

    if( green_button.isPressed() ){
        syslog.logf(LOG_INFO, "Green Button Pressed %lu\n", green_button.heldDuration());
        syslog.logf(LOG_INFO, "Green Button duration %lu\n", green_button.getDuration());
    }

    // syslog.logf(LOG_DEBUG, "Green Button Pressed %s Held Duration %lu\n", green_button.isPressed() == true ? "true" : "false", green_button.heldDuration());
    // syslog.logf(LOG_DEBUG, "Green Button duration %lu\n", green_button.getDuration());
    
    // syslog.logf(LOG_DEBUG, "held button check\n");
    if( green_button.isReleased() && green_button.getDuration() > 3000 ) {
        syslog.logf(LOG_INFO, "Green Button Held\n");
        scale.tare();

        green_led.blinkNumberOfTimes(250, 250, 4);

        green_button.resetDuration();
    }

    if( yellow_button.isMultipressComplete() && yellow_button.getCount() > 0 ) {
        syslog.logf(LOG_INFO, "Press Count: %d", yellow_button.getCount());

        yellow_button.resetMultipress();
    }

    scale_state_new = pet_weight.get_state();

    if( scale_state != scale_state_new) {
        syslog.logf(LOG_NOTICE, "Scale State %s\n", pet_weight.get_state_str());
        scale_state = scale_state_new;

        if( scale_state == PetWeight::ScaleState::pet_in_box ) {
            red_led.breathe(0, 255, 2000);
        }
    }

    #ifdef CALIBRATE
    if( CALIBRATE == 1 ) {        
        scale.set_scale(calibration_factor); //Adjust to this calibration factor

        Serial.print("Reading: ");
        Serial.print(scale.get_units(), 3);
        Serial.print(" lbs"); //Change this to kg and re-adjust the calibration factor if you follow SI units like a sane person
        Serial.print(" calibration_factor: ");
        Serial.print(calibration_factor);
        Serial.println();

        if(Serial.available())
        {
            char temp = Serial.read();
            if(temp == '+' || temp == 'a') {
                calibration_factor += 10;
            } else if(temp == '-' || temp == 'z') {
                calibration_factor -= 10;
            } else if(temp == 's') {
                calibration_factor += 100;
            } else if(temp == 'x') {
                calibration_factor -= 100;
            } else if(temp == 'd') {
                calibration_factor += 1000;
            } else if(temp == 'c') {
                calibration_factor -= 1000;
            } else if(temp == 'f') {
                calibration_factor += 10000;
            } else if(temp == 'v') {
                calibration_factor -= 10000;
            } else if(temp == 'g') {
                calibration_factor += 100000;
            } else if(temp == 'b') {
                calibration_factor -= 100000;
            } else if(temp == 'p') {
                pref_result = prefs.putFloat("cal_factor", calibration_factor);
                Serial.printf("Set calibration factor size: %d\n", pref_result);
                Serial.println("Get Cal Factor Pref");
                calibration_factor = prefs.getFloat("cal_factor");
                Serial.printf("Done Get Cal Factor Pref: %f, size: %d\n", calibration_factor, pref_result);
            }
        }
        delay(500);
    }
    #endif

}



