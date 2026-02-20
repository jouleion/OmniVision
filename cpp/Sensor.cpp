#include "Sensor.h"

Sensor::Sensor(const char* name, uint8_t pin)
    : _name(name), _pin(pin), _temperature(0.0f), _humidity(0.0f),
      _calibOffset(0.0f), _ready(false) {}

void Sensor::update() {
    // TODO: Replace with actual sensor-specific read logic (e.g., DHT, BMP280).
    // For now we simulate a reading using analogRead scaled to a plausible range.
    static const float ADC_MAX = 4095.0f; // ESP32 12-bit ADC: values 0–4095
    int raw = analogRead(_pin);
    _temperature = (raw / ADC_MAX) * 50.0f + _calibOffset; // 0–50 °C range
    _humidity    = (raw / ADC_MAX) * 100.0f;                // 0–100 % range
    _ready       = true;
}

// --- Getters ---

const char* Sensor::getName() const { return _name; }

uint8_t Sensor::getPin() const { return _pin; }

float Sensor::getTemperature() const { return _temperature; }

float Sensor::getHumidity() const { return _humidity; }

bool Sensor::isReady() const { return _ready; }

// --- Setters ---

void Sensor::setName(const char* name) { _name = name; }

void Sensor::setPin(uint8_t pin) { _pin = pin; }

void Sensor::setCalibrationOffset(float offset) { _calibOffset = offset; }

// --- Serial output ---

String Sensor::toSerialString() const {
    String s = "{";
    s += "\"sensor\":\"";  s += _name;             s += "\",";
    s += "\"pin\":";       s += _pin;               s += ",";
    s += "\"temp\":";      s += String(_temperature, 2); s += ",";
    s += "\"humidity\":";  s += String(_humidity, 2);    s += ",";
    s += "\"ready\":";     s += (_ready ? "true" : "false");
    s += "}";
    return s;
}
