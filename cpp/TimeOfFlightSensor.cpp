#include "TimeOfFlightSensor.h"

TimeOfFlightSensor::TimeOfFlightSensor(const uint8_t id, uint8_t pin) : 
    _id(id), _pin(pin) {

}

bool TimeOfFlightSensor::begin() {
    pinMode(_pin, INPUT);
}

int TimeOfFlightSensor::read() {
    return 1; // Placeholder for actual read logic
}

bool TimeOfFlightSensor::write(int value) {
    return true; // Placeholder for actual write logic
}
