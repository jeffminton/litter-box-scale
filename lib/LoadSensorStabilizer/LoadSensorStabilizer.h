#ifndef LOAD_SENSOR_STABILIZER_H
#define LOAD_SENSOR_STABILIZER_H

#include <vector>
#include <cstddef>

class LoadSensorStabilizer {
private:
    size_t windowSize;
    double stabilityThreshold;
    std::vector<double> readings;
    size_t insertIndex;
    bool bufferFull;

public:
    // windowSize: number of consecutive samples to evaluate
    // threshold: maximum allowed standard deviation to consider "steady"
    LoadSensorStabilizer(size_t windowSize, double threshold);

    // Adds a new sensor reading to the rolling buffer
    void addReading(double value);

    // Calculates current stability status
    bool isSteady() const;

    // Helper to get average weight when stable
    double getAverage() const;
};

#endif // LOAD_SENSOR_STABILIZER_H
