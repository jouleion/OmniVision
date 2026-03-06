#pragma once

// This is the VL53L7CX ToF Sensor
// https://www.st.com/resource/en/datasheet/vl53l7cx.pdf
//  matrix of 8x8 zones (64 zones) and can work at fast speeds (60 Hz) up to 350 cm.


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
