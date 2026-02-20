#pragma once


// Simplified sensor class for TOF sensor
class TimeOfFlightSensor {
public:
    TimeOfFlightSensor(const uint8_t id, uint8_t pin);

    // Initialize the sensor
    bool begin();

    // Boilerplate functions for the TOF sensor
    int read();
    bool write(int value);

private:
    const uint8_t     _id;
    uint8_t     _pin;
};
