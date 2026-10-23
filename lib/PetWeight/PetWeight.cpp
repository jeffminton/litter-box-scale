#include "PetWeight.h"


PetWeight::PetWeight()
    : pet_weights_length(PET_WEIGHTS_LENGTH)
{
    prefs.begin("pet-scale"); // use "my-app" namespace

    // prefs.remove("pet_weights");

    if (not prefs.isKey("pet_weights")) {
        // pet_weights[0] = {"Kaylee", 12.5}; // one
        prefs.putBytes("pet_weights", pet_weights, sizeof(pet_weights));
    }

    size_t pw_pref_len = prefs.getBytesLength("pet_weights");
    // simple check that data fits
    if (0 == pw_pref_len || pw_pref_len % sizeof(pet_weight_t) || pw_pref_len > sizeof(pet_weights)) {
        Serial.printf("Invalid size of pet_weights array: %zu, should be %zu\n", pw_pref_len, sizeof(pet_weights));
        // GraylogLogger.printf("Invalid size of pet_weights array: %zu, should be %zu\n", pw_pref_len, sizeof(pet_weights));
        prefs.putBytes("pet_weights", pet_weights, sizeof(pet_weights));
        return;
    }
    prefs.getBytes("pet_weights", pet_weights, pw_pref_len);

    // GraylogLogger.println("From PetWeights Using Graylog Logger");
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
    Serial.printf("update_weight: index: %d, weight: %.3f\n", index, weight);
    WebSerial.printf("update_weight: index: %d, weight: %.3f\n", index, weight);
    GraylogLogger.printf("update_weight: index: %d, weight: %.3f\n", index, weight);
    int current_index;
    current_index = pet_weights[index].current_index + 1;

    Serial.printf("update_weight: current_index: %d\n", current_index);
    WebSerial.printf("update_weight: current_index: %d\n", current_index);
    GraylogLogger.printf("update_weight: current_index: %d\n", current_index);

    time(&now);                       // read the current time
    localtime_r(&now, &tm);

    if( current_index >= PET_WEIGHT_MAX_RECORDS ) {
        current_index = 0;
    }
    
    Serial.printf("update_weight: set weight: %.3f\n", weight);
    WebSerial.printf("update_weight: set weight: %.3f\n", weight);
    GraylogLogger.printf("update_weight: set weight: %.3f\n", weight);
    pet_weights[index].weight[current_index] = weight;
    Serial.printf("update_weight: set timestamp: %lld\n", (long long)now);
    WebSerial.printf("update_weight: set timestamp: %lld\n", (long long)now);
    GraylogLogger.printf("update_weight: set timestamp: %lld\n", (long long)now);
    pet_weights[index].timestamp[current_index] = now;
    Serial.printf("update_weight: set current_index: %d\n", current_index);
    WebSerial.printf("update_weight: set current_index: %d\n", current_index);
    GraylogLogger.printf("update_weight: set current_index: %d\n", current_index);
    pet_weights[index].current_index = current_index;
    Serial.printf("update_weight: DONE\n");
    WebSerial.printf("update_weight: DONE\n");
    GraylogLogger.printf("update_weight: DONE\n");

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
        Serial.printf("current_index: %d, current_weight: %f\n", current_index, current_weight);
        WebSerial.printf("current_index: %d, current_weight: %f\n", current_index, current_weight);
        GraylogLogger.printf("current_index: %d, current_weight: %f\n", current_index, current_weight);

        if( current_weight > -1) {
            current_weight_diff = abs(current_weight - weight);
            if( (min_diff == -1 || current_weight_diff < min_diff) && current_weight_diff <= min_weight_match_delta ) {
                min_diff = current_weight_diff;
                match_index = current_index;
            }
        } else {
            // current_index points at an empt pet slot so return 1 for a new non matching pet
            Serial.printf("No Match, Return index %d\n", current_index);
            WebSerial.printf("No Match, Return index %d\n", current_index);
            GraylogLogger.printf("No Match, Return index %d\n", current_index);
            return current_index;
            Serial.printf("!!!! Still in base no match. shouldn't be here\n");
            WebSerial.printf("!!!! Still in base no match. shouldn't be here\n");
            GraylogLogger.printf("!!!! Still in base no match. shouldn't be here\n");
        }
    }

    if( match_index == -1 ) {
        return current_index;
    }

    return match_index;
}



void PetWeight::log_all_pet_weights_string(){
    for(int i = 0; i < pet_weights_length; i++) {
        Serial.printf("%s: %.2f\n", pet_weights[i].name.c_str(), get_pet_current_weight(i));
        WebSerial.printf("%s: %.2f\n", pet_weights[i].name.c_str(), get_pet_current_weight(i));
        GraylogLogger.printf("%s: %.2f\n", pet_weights[i].name.c_str(), get_pet_current_weight(i));
    }
}






void PetWeight::process_recorded_weight(float current_weight) {

    if( previous_weight == -1 ) {
        previous_weight = current_weight;
    }

    if ( current_weight > (previous_weight + weight_changed_threshold) || current_weight < (previous_weight - weight_changed_threshold) ) {
        Serial.printf("Steady Value: count: %d, millis: %lu %.3f, %.3f lbs\n", sample_counter, millis() - start_time, previous_weight, current_weight);
        WebSerial.printf("Steady Value: count: %d, millis: %lu %.3f, %.3f lbs\n", sample_counter, millis() - start_time, previous_weight, current_weight);
        GraylogLogger.printf("Steady Value: count: %d, millis: %lu %.3f, %.3f lbs\n", sample_counter, millis() - start_time, previous_weight, current_weight);

        previous_weight = current_weight;
        if( current_weight > pet_weight_in_box) {
            pet_weight_in_box = current_weight;
            pet_arrival_time = millis();
            Serial.printf("pet_arrival_time: %lu, pet_weight_in_box: %.3f\n", pet_arrival_time / 1000, pet_weight_in_box);
            WebSerial.printf("pet_arrival_time: %lu, pet_weight_in_box: %.3f\n", pet_arrival_time / 1000, pet_weight_in_box);
            GraylogLogger.printf("pet_arrival_time: %lu, pet_weight_in_box: %.3f\n", pet_arrival_time / 1000, pet_weight_in_box);
            pet_in_box = true;
        } else {
            weight_after_pet = current_weight;
            pet_depart_time = millis();
            Serial.printf("pet_depart_time: %lu, weight_after_pet: %.3f\n", (unsigned long)(pet_depart_time / 1000), weight_after_pet);
            WebSerial.printf("pet_depart_time: %lu, weight_after_pet: %.3f\n", (unsigned long)(pet_depart_time / 1000), weight_after_pet);
            GraylogLogger.printf("pet_depart_time: %lu, weight_after_pet: %.3f\n", (unsigned long)(pet_depart_time / 1000), weight_after_pet);
            pet_measured = true;
            pet_in_box = false;
        }
        
    }
    sample_counter = 0;

    elapsed_seconds = ( ( millis() - pet_arrival_time ) / 1000 );
    if( elapsed_seconds % 10 == 0 && elapsed_seconds != last_printed_seconds && (pet_measured == true || pet_in_box == true) ) {

        Serial.printf("Since arival seconds: %d\n", elapsed_seconds);
        WebSerial.printf("Since arival seconds: %d\n", elapsed_seconds);
        GraylogLogger.printf("Since arival seconds: %d\n", elapsed_seconds);
        last_printed_seconds = elapsed_seconds;
    }

    if( pet_measured == true && ( millis() - pet_arrival_time ) / 1000 >= pet_visit_bound_seconds) {
        // try {
            Serial.printf("record_pet_loop_count: %d\n", record_pet_loop_count);
            WebSerial.printf("record_pet_loop_count: %d\n", record_pet_loop_count);
            GraylogLogger.printf("record_pet_loop_count: %d\n", record_pet_loop_count);
            record_pet_loop_count++;
            Serial.printf("Complete measurment: %lu\n", ( millis() - pet_arrival_time ) / 1000);
            WebSerial.printf("Complete measurment: %lu\n", ( millis() - pet_arrival_time ) / 1000);
            GraylogLogger.printf("Complete measurment: %lu\n", ( millis() - pet_arrival_time ) / 1000);
            pet_visit_seconds_total = ( pet_depart_time - pet_arrival_time ) / 1000;
            Serial.printf("pet_visit_seconds_total: %lu\n", pet_visit_seconds_total);
            WebSerial.printf("pet_visit_seconds_total: %lu\n", pet_visit_seconds_total);
            GraylogLogger.printf("pet_visit_seconds_total: %lu\n", pet_visit_seconds_total);
            pet_final_weight = pet_weight_in_box - weight_after_pet;
            Serial.printf("pet_final_weight: %f\n", pet_final_weight);
            WebSerial.printf("pet_final_weight: %f\n", pet_final_weight);
            GraylogLogger.printf("pet_final_weight: %f\n", pet_final_weight);
            if( pet_visit_seconds_total > min_visit_seconds && pet_final_weight > min_valid_pet_weight ) {
                Serial.printf("weight valid:\n");
                WebSerial.printf("weight valid:\n");
                GraylogLogger.printf("weight valid:\n");
                matching_pet_index = get_matching_pet_index(pet_final_weight);
                Serial.printf("matching_pet_index: %d\n", matching_pet_index);
                WebSerial.printf("matching_pet_index: %d\n", matching_pet_index);
                GraylogLogger.printf("matching_pet_index: %d\n", matching_pet_index);
                update_weight(matching_pet_index, pet_final_weight);
                Serial.printf("update_weight:\n");
                WebSerial.printf("update_weight:\n");
                GraylogLogger.printf("update_weight:\n");
                log_all_pet_weights_string();
                Serial.printf("log_all_pet_weights_string:\n");
                WebSerial.printf("log_all_pet_weights_string:\n");
                GraylogLogger.printf("log_all_pet_weights_string:\n");
            } else {
                pet_weight_in_box = 0;
            }
            pet_measured = false;
        // } catch (const std::exception& e) { 
        //     // Catches any exception derived from std::exception
        //     Serial.printf( "Standard Exception Caught: %s\n", e.what() );
        //     WebSerial.printf( "Standard Exception Caught: %s\n", e.what() );
        // }
    }
}