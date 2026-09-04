#include "LoadSensorStabilizer.h"
#include <numeric>
#include <cmath>

LoadSensorStabilizer::LoadSensorStabilizer(size_t windowSize, double threshold)
    : windowSize(windowSize), stabilityThreshold(threshold), insertIndex(0), bufferFull(false) {
    readings.resize(windowSize, 0.0);
}

void LoadSensorStabilizer::addReading(double value) {
    readings[insertIndex] = value;
    insertIndex = (insertIndex + 1) % windowSize;
    if (insertIndex == 0) {
        bufferFull = true;
    }
}

bool LoadSensorStabilizer::isSteady() const {
    // Require a full buffer of samples before making a determination
    if (!bufferFull && insertIndex < windowSize) {
        return false;
    }

    double sum = std::accumulate(readings.begin(), readings.end(), 0.0);
    double mean = sum / windowSize;

    double accum = 0.0;
    for (double val : readings) {
        accum += (val - mean) * (val - mean);
    }

    double stdev = std::sqrt(accum / windowSize);
    return stdev <= stabilityThreshold;
}

double LoadSensorStabilizer::getAverage() const {
    double sum = std::accumulate(readings.begin(), readings.end(), 0.0);
    return sum / windowSize;
}
