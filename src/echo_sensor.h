// class header for Echo sensor object.
#pragma once

#include <Arduino.h>

class EchoSensor {
    public:
        EchoSensor(uint8_t pin);
        bool begin();
        uint16_t fetchRawData();

    private:
        uint8_t pin;
        
};
