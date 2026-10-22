#ifndef PET_WEIGHT_H
#define PET_WEIGHT_H

#include <Preferences.h>
#include <WebSerial.h>
#include <time.h>                   // for time() ctime()

#include <GelfLogger.h>

// #include <UDPTransport.h>
// #include <Message.h>
// #include <Publisher.h>

#define PET_WEIGHT_MAX_RECORDS 64
#define PET_WEIGHTS_LENGTH 16

typedef struct {
    String name = "None";
    float weight[PET_WEIGHT_MAX_RECORDS];
    time_t timestamp[PET_WEIGHT_MAX_RECORDS];
    int current_index = -1;
} pet_weight_t;


class PetWeight {
private:
    const int pet_weights_length;

    time_t now;                         // this are the seconds since Epoch (1970) - UTC
    struct tm tm;                              // the structure tm holds time information in a more convenient way

    Preferences prefs;

    float min_weight_match_delta;

    double previous_weight = -1;
    double current_weight = 0;

    float weight_changed_threshold = 0.5;

    int sample_counter = 0;

    unsigned long start_time;

    float pet_weight_in_box = 0;

    unsigned long pet_arrival_time, pet_depart_time, pet_visit_seconds_total;

    bool pet_measured = false, pet_in_box = false;

    float weight_after_pet = 0;

    int elapsed_seconds = 0;

    int last_printed_seconds = 0;

    const int pet_visit_bound_seconds = 60;

    int record_pet_loop_count = 0;

    float pet_final_weight = 0;

    const int min_visit_seconds = 30;

    int matching_pet_index = -1;

    const float min_valid_pet_weight = 6.0;

    pet_weight_t pet_weights[PET_WEIGHTS_LENGTH]; // buffer for the data (max 16 entries)

public:
    PetWeight();

    int get_pet_index(String pet_name);
    
    int update_weight(int index, float weight);
    
    float get_pet_current_weight(int index);
    
    int get_matching_pet_index(float weight);
    
    void log_all_pet_weights_string();

    void process_recorded_weight(float current_weight);
};

#endif // LOAD_SENSOR_STABILIZER_H
