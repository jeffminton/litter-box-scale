
// #include <WiFiClient.h>
// #include <ArduinoOTA.h>
#include <WiFiUdp.h>
// include library to read and write from flash memory
#include <EEPROM.h>
// #include <uri/UriBraces.h>
// #include <uri/UriRegex.h>
#include <time.h>                   // for time() ctime()
// #include <NTPClient.h>
#include <WebSerial.h>

// This are the graylog dependencies
#include <UDPTransport.h>
#include <Message.h>
#include <Publisher.h>

#include "HX711.h"

#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h> // For parsing API data

#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

#include <AsyncTCP.h>

#include <ElegantOTA.h>

#include <Preferences.h>

#include <LoadSensorStabilizer.h>

#define calibration_factor 9940.0 //This value is obtained using the SparkFun_HX711_Calibration sketch

#define DOUT  23
#define CLK  22

#define PET_WEIGHT_MAX_RECORDS 64

typedef struct {
  String name = "None";
  float weight[PET_WEIGHT_MAX_RECORDS];
  time_t timestamp[PET_WEIGHT_MAX_RECORDS];
  int current_index = -1;
} pet_weight_t;

const int pet_weights_length = 16;

pet_weight_t pet_weights[pet_weights_length]; // buffer for the data (max 16 entries)


Preferences prefs;

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

double previous_weight = -1;
double current_weight = 0;

/* Configuration of NTP */
#define MY_NTP_SERVER "pool.ntp.org"
#define MY_TZ "EST5EDT,M3.2.0,M11.1.0"

const long ntp_update_delay = 10000;
long last_ntp_update = -1;
const long utcOffsetInSeconds = -25200;

time_t now;                         // this are the seconds since Epoch (1970) - UTC
tm tm;                              // the structure tm holds time information in a more convenient way


time_t timestamp;

unsigned long start_time;

int current_index;

const int steady_weight_timeout_ms = 2000;
const float steady_weight_tolerance = 0.01;
const int steady_weight_count_required = 20;

int sample_counter = 0;

float weight_changed_threshold = 0.5;

LoadSensorStabilizer sensor(steady_weight_count_required, steady_weight_tolerance);

float pet_weight_in_box = 0;
float pet_final_weight = 0;
float weight_after_pet = 0;
float measured_pet_wieght = 0;
const float min_valid_pet_weight = 6.0;
const int min_visit_seconds = 30;
unsigned long pet_arrival_time, pet_depart_time, pet_visit_seconds_total;
const int pet_visit_bound_seconds = 600;

int matching_pet_index = -1;


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


int get_pet_index(String pet_name, pet_weight_t *pet_weights) {
    for(int i; i < pet_weights_length; i++) {
        if( pet_weights[i].name == pet_name ) {
            return i;
        }

        return -1;
    }
}


int update_weight(int index, float weight, pet_weight_t *pet_weights) {
    int current_index;
    current_index = pet_weights[index].current_index + 1;

    time(&now);                       // read the current time
    localtime_r(&now, &tm);

    if( current_index >= PET_WEIGHT_MAX_RECORDS ) {
        current_index = 0;
    }

    pet_weights[index].weight[current_index] = weight;
    pet_weights[index].timestamp[current_index] = now;
    pet_weights[index].current_index = current_index;
}


int get_pet_current_weight(int index, pet_weight_t *pet_weights) {
    int current_index;
    current_index = pet_weights[index].current_index;

    if( current_index >= 0 ) {
        return pet_weights[index].weight[current_index];
    } else {
        return -1;
    }
}



int get_matching_pet_index(float weight, pet_weight_t *pet_weights) {
    int match_index = -1;
    float min_diff = -1;
    float current_weight_diff;
    float current_weight;
    
    for(int i; i < pet_weights_length; i++) {
        current_weight = get_pet_current_weight(i, pet_weights);

        if( current_weight > -1) {
            current_weight_diff = abs(current_weight - weight);
            if( min_diff == -1 || current_weight_diff < min_diff ) {
                min_diff = current_weight_diff;
                return i;
            }
        } else {
            // i points at an empt pet slot so return 1 for a new non matching pet
            return i;
        }
    }

    return match_index;
}



void log_all_pet_weights_string(pet_weight_t *pet_weights){
    for(int i; i < pet_weights_length; i++) {
        Serial.printf("%s: %.2f", pet_weights[i].name, get_pet_current_weight(i, pet_weights));
        WebSerial.printf("%s: %.2f", pet_weights[i].name, get_pet_current_weight(i, pet_weights));
    }
}


void setup() {
    Serial.begin(57600);

    // WiFi.begin(ssid, password);     //Connect to your WiFi router
    Serial.println("");

    configTzTime(MY_TZ, MY_NTP_SERVER); // --> Here is the IMPORTANT ONE LINER needed in your sketch!

    // Initialize LittleFS
    if(!LittleFS.begin()){
        Serial.println("An Error has occurred while mounting LittleFS");
        return;
    }

    prefs.begin("pet-scale"); // use "my-app" namespace

    // prefs.remove("pet_weights");

    if (not prefs.isKey("pet_weights")) {
        // pet_weights[0] = {"Kaylee", 12.5}; // one
        prefs.putBytes("pet_weights", pet_weights, sizeof(pet_weights));
    }

    size_t pw_pref_len = prefs.getBytesLength("pet_weights");
    // simple check that data fits
    if (0 == pw_pref_len || pw_pref_len % sizeof(pet_weight_t) || pw_pref_len > sizeof(pet_weights)) {
        Serial.printf("Invalid size of pet_weights array: %zu\n", pw_pref_len);
        return;
    }
    prefs.getBytes("pet_weights", pet_weights, pw_pref_len);


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


    Serial.println("HX711 scale demo");

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

    Serial.println("Readings:");


    start_time = millis();
}

void loop() {
    // ArduinoOTA.handle();
    ElegantOTA.loop();
    yield();

    time(&now);                       // read the current time
    localtime_r(&now, &tm);

    // sensor.addReading(current_weight);

    // while( sensor.isSteady() == false ) {
    current_weight = scale.get_units();
    sensor.addReading(current_weight);
    sample_counter++;
    // }

    if( sensor.isSteady() == true ) {
        if ( current_weight > (previous_weight + weight_changed_threshold) || current_weight < (previous_weight - weight_changed_threshold) ) {
            Serial.printf("Steady Value: count: %d, millis: %d %.3f, %.3f lbs\n", sample_counter, millis() - start_time, previous_weight, current_weight);

            previous_weight = current_weight;
            if( current_weight > pet_weight_in_box) {
                pet_weight_in_box = current_weight;
                pet_arrival_time = millis();
                Serial.printf("Arrival time: %d\n", pet_arrival_time / 1000);
            } else {
                weight_after_pet = current_weight;
                pet_depart_time = millis();
                Serial.printf("Depart time: %d\n", pet_depart_time / 1000);
            }
            
        }
        sample_counter = 0;

        if( ( ( millis() - pet_arrival_time ) / 1000 ) % 10 == 0) {
            Serial.printf("Bound seconds: %d\n", ( millis() - pet_arrival_time ) / 1000);
        }

        if( ( millis() - pet_arrival_time ) / 1000 >= pet_visit_bound_seconds) {
            Serial.printf("Bound seconds: %d\n", ( millis() - pet_arrival_time ) / 1000);
            pet_visit_seconds_total = ( pet_depart_time - pet_arrival_time ) / 1000;
            pet_final_weight = pet_weight_in_box - weight_after_pet;
            if( pet_visit_seconds_total > min_visit_seconds && pet_final_weight > min_valid_pet_weight ) {
                matching_pet_index = get_matching_pet_index(pet_final_weight, pet_weights);
                update_weight(matching_pet_index, pet_final_weight, pet_weights);
                log_all_pet_weights_string(pet_weights);
            } else {
                pet_weight_in_box = 0;
            }
        }
    }

}



