#include "PetWeight.h"


PetWeight::PetWeight( Syslog& log ):
    pet_weights_length(PET_WEIGHTS_LENGTH),
    syslog(log)
{
    prefs.begin("pet-scale"); // use "my-app" namespace

    // // prefs.remove("pet_weights");

    // if (not prefs.isKey("pet_weights")) {
    //     // pet_weights[0] = {"Kaylee", 12.5}; // one
    //     prefs.putBytes("pet_weights", pet_weights, sizeof(pet_weights));
    // }

    // size_t pw_pref_len = prefs.getBytesLength("pet_weights");
    // // simple check that data fits
    // if (0 == pw_pref_len || pw_pref_len % sizeof(pet_weight_t) || pw_pref_len > sizeof(pet_weights)) {
    //     syslog.logf(LOG_DEBUG, "Invalid size of pet_weights array: %zu, should be %zu\n", pw_pref_len, sizeof(pet_weights));
    //     prefs.putBytes("pet_weights", pet_weights, sizeof(pet_weights));
    //     return;
    // }
    // prefs.getBytes("pet_weights", pet_weights, pw_pref_len);

    // // GraylogLogger.println("From PetWeights Using Graylog Logger");

    read_preference_values();
}

int PetWeight::get_pet_index(String pet_name) {
    for(int i = 0; i < pet_weights_length; i++) {
        if( pet_weights[i].name == pet_name ) {
            return i;
        }
    }

    return -1;
}


int PetWeight::update_weight(int index, float weight) {
    syslog.logf(LOG_NOTICE, "update_weight: index: %d, weight: %.3f\n", index, weight);
    int current_index;
    current_index = pet_weights[index].current_index + 1;

    syslog.logf(LOG_INFO, "update_weight: current_index: %d\n", current_index);

    time(&now);                       // read the current time
    localtime_r(&now, &tm);

    if( current_index >= PET_WEIGHT_MAX_RECORDS ) {
        current_index = 0;
    }
    
    syslog.logf(LOG_DEBUG, "update_weight: set weight: %.3f\n", weight);
    pet_weights[index].weight[current_index] = weight;
    syslog.logf(LOG_DEBUG, "update_weight: set timestamp: %lld\n", (long long)now);
    pet_weights[index].timestamp[current_index] = now;
    syslog.logf(LOG_DEBUG, "update_weight: set current_index: %d\n", current_index);
    pet_weights[index].current_index = current_index;
    syslog.logf(LOG_DEBUG, "update_weight: DONE\n");

    prefs.putBytes("pet_weights", pet_weights, sizeof(*pet_weights));

    return current_index;
}


float PetWeight::get_pet_current_weight(int index) {
    int current_index;
    current_index = pet_weights[index].current_index;

    if( current_index >= 0 ) {
        return pet_weights[index].weight[current_index];
    } else {
        return -1;
    }
}



int PetWeight::get_matching_pet_index(float weight) {
    int match_index = -1;
    float min_diff = -1;
    float current_weight_diff;
    float current_weight;
    int current_index = 0;
    
    for(int current_index = 0; current_index < pet_weights_length; current_index++) {
        current_weight = get_pet_current_weight(current_index);
        syslog.logf(LOG_INFO, "current_index: %d, current_weight: %f\n", current_index, current_weight);

        if( current_weight > -1) {
            current_weight_diff = abs(current_weight - weight);
            if( (min_diff == -1 || current_weight_diff < min_diff) && current_weight_diff <= min_weight_match_delta ) {
                min_diff = current_weight_diff;
                match_index = current_index;
            }
        } else {
            // current_index points at an empt pet slot so return 1 for a new non matching pet
            syslog.logf(LOG_INFO, "No Match, Return index %d\n", current_index);
            return current_index;
            syslog.logf(LOG_INFO, "!!!! Still in base no match. shouldn't be here\n");
        }
    }

    if( match_index == -1 ) {
        return current_index;
    }

    return match_index;
}



void PetWeight::log_all_pet_weights_string(){
    for(int i = 0; i < pet_weights_length; i++) {
        syslog.logf(LOG_NOTICE, "%s: %.2f\n", pet_weights[i].name.c_str(), get_pet_current_weight(i));
    }
}


void PetWeight::read_preference_values() {
    // prefs.remove("pet_weights");

    if (not prefs.isKey("pet_weights")) {
        // pet_weights[0] = {"Kaylee", 12.5}; // one
        prefs.putBytes("pet_weights", pet_weights, sizeof(pet_weights));
    }

    size_t pw_pref_len = prefs.getBytesLength("pet_weights");
    // simple check that data fits
    if (0 == pw_pref_len || pw_pref_len % sizeof(pet_weight_t) || pw_pref_len > sizeof(pet_weights)) {
        syslog.logf(LOG_ERR, "Invalid size of pet_weights array: %zu, should be %zu\n", pw_pref_len, sizeof(pet_weights));
        prefs.putBytes("pet_weights", pet_weights, sizeof(pet_weights));
    }

    prefs.getBytes("pet_weights", pet_weights, pw_pref_len);

    // GraylogLogger.println("From PetWeights Using Graylog Logger");

    weight_changed_threshold = prefs.getFloat("wct", weight_changed_threshold);

    pet_visit_bound_seconds = prefs.getInt("pvbs", pet_visit_bound_seconds);

    min_visit_seconds = prefs.getInt("mvs", min_visit_seconds);

    min_valid_pet_weight = prefs.getFloat("mvpw", min_valid_pet_weight);

    measure_timeout_seconds = prefs.getInt("mts", measure_timeout_seconds);
}




void PetWeight::save_preference_values() {
    // prefs.remove("pet_weights");

    prefs.putBytes("pet_weights", pet_weights, sizeof(pet_weights));
    
    // GraylogLogger.println("From PetWeights Using Graylog Logger");

    prefs.putFloat("wct", weight_changed_threshold);

    prefs.putInt("pvbs", pet_visit_bound_seconds);

    prefs.putInt("mvs", min_visit_seconds);

    prefs.putFloat("mvpw", min_valid_pet_weight);

    prefs.putInt("mts", measure_timeout_seconds);
}




void PetWeight::process_recorded_weight(float current_weight) {

    if( previous_weight == -1 ) {
        previous_weight = current_weight;
    }

    if ( current_weight > (previous_weight + weight_changed_threshold) || current_weight < (previous_weight - weight_changed_threshold) ) {
        syslog.logf(LOG_INFO, "Steady Value: count: %d, millis: %lu prev weight: %.3f, curr weight: %.3f lbs\n", sample_counter, millis() - start_time, previous_weight, current_weight);

        previous_weight = current_weight;
        if( current_weight > pet_weight_in_box) {
            pet_weight_in_box = current_weight;
            pet_arrival_time = millis();
            syslog.logf(LOG_NOTICE, "pet_arrival_time: %lu, pet_weight_in_box: %.3f\n", pet_arrival_time / 1000, pet_weight_in_box);
            scale_state = ScaleState::pet_in_box;
            // pet_in_box = true;
        } else {
            weight_after_pet = current_weight;
            pet_depart_time = millis();
            syslog.logf(LOG_NOTICE, "pet_depart_time: %lu, weight_after_pet: %.3f\n", (unsigned long)(pet_depart_time / 1000), weight_after_pet);
            scale_state = ScaleState::pet_measured;
            // pet_measured = true;
            // pet_in_box = false;
        }
        
    }
    sample_counter = 0;

    elapsed_seconds = ( ( millis() - pet_arrival_time ) / 1000 );
    if( elapsed_seconds % 10 == 0 && elapsed_seconds != last_printed_seconds && 
        (scale_state == ScaleState::pet_measured || scale_state == ScaleState::pet_in_box) ) {

        syslog.logf(LOG_INFO, "Since arival seconds: %d\n", elapsed_seconds);
        last_printed_seconds = elapsed_seconds;
    }



    if( scale_state == ScaleState::pet_measured && ( millis() - pet_arrival_time ) / 1000 >= pet_visit_bound_seconds) {
        // try {
            syslog.logf(LOG_DEBUG, "record_pet_loop_count: %d\n", record_pet_loop_count);
            record_pet_loop_count++;
            syslog.logf(LOG_DEBUG, "Complete measurment: %lu\n", ( millis() - pet_arrival_time ) / 1000);
            pet_visit_seconds_total = ( pet_depart_time - pet_arrival_time ) / 1000;
            syslog.logf(LOG_DEBUG, "pet_visit_seconds_total: %lu\n", pet_visit_seconds_total);
            pet_final_weight = pet_weight_in_box - weight_after_pet;
            syslog.logf(LOG_DEBUG, "pet_final_weight: %f\n", pet_final_weight);
            if( pet_visit_seconds_total > min_visit_seconds && pet_final_weight > min_valid_pet_weight ) {
                syslog.logf(LOG_DEBUG, "weight valid:\n");
                matching_pet_index = get_matching_pet_index(pet_final_weight);
                syslog.logf(LOG_DEBUG, "matching_pet_index: %d\n", matching_pet_index);
                update_weight(matching_pet_index, pet_final_weight);
                syslog.logf(LOG_DEBUG, "update_weight:\n");
                log_all_pet_weights_string();
                syslog.logf(LOG_DEBUG, "log_all_pet_weights_string:\n");
            } else {
                pet_weight_in_box = 0;
            }
            // pet_measured = false;
            scale_state = ScaleState::waiting_for_pet;
        // } catch (const std::exception& e) { 
        //     // Catches any exception derived from std::exception
        // }
    }
}

PetWeight::ScaleState PetWeight::get_state() {
    return scale_state;
}


char* PetWeight::get_state_str() {
    switch (scale_state) {
        case ScaleState::pet_in_box:        return "Pet In Box";
        case ScaleState::pet_measured:      return "Pet Measured";
        case ScaleState::waiting_for_pet:   return "Waiting For Pet";
        default:                            return "Unknown";
    }
}
