// implementation of Echo sensor object.
#include "echo_sensor.h"

EchoSensor::EchoSensor(uint8_t pin) :
    pin(pin)
{
}

bool EchoSensor::begin() {
    // Initialize pin or hardware as needed
    pinMode(pin, INPUT);
    return true;
}

uint16_t EchoSensor::fetchRawData() {
    // Placeholder: read sensor value; return 0 if not available
    return static_cast<uint16_t>(pulseIn(pin, HIGH));
}