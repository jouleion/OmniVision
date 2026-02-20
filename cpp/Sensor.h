#pragma once

#include <Arduino.h>

// Example sensor class representing a generic environmental sensor.
// Demonstrates encapsulation with getters and setters.
class Sensor {
public:
    Sensor(const char* name, uint8_t pin);

    // Read the physical sensor and update internal state
    void update();

    // Getters
    const char* getName() const;
    uint8_t     getPin() const;
    float       getTemperature() const;
    float       getHumidity() const;
    bool        isReady() const;

    // Setters
    void setName(const char* name);
    void setPin(uint8_t pin);
    void setCalibrationOffset(float offset);

    // Serialize current reading to a JSON-like string for Serial output
    String toSerialString() const;

private:
    const char* _name;
    uint8_t     _pin;
    float       _temperature;   // degrees Celsius
    float       _humidity;      // percentage (0-100)
    float       _calibOffset;   // temperature calibration offset
    bool        _ready;
};
