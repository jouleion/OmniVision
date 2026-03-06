#include "IntertialMeasurementUnit.h"

IMU::IMU(const uint8_t id, uint8_t pin) : 
    _id(id), _pin(pin) {

}

bool IMU::begin() {
    pinMode(_pin, INPUT);
}

int IMU::read() {
    return 1; // Placeholder for actual read logic
}

bool IMU::write(int value) {
    return true; // Placeholder for actual write logic
}
