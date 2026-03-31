// class header for Echo sensor object.
#pragma once

#include <Arduino.h>

class EchoSensor {
    public:
        EchoSensor(uint8_t trig_pin, uint8_t echo_pin);
        bool begin();
        uint16_t getDistanceCM();
        bool newDataAvailable();
        bool timedOut();
        void trigger();                 // for interrupt
        static void IRAM_ATTR handleInterrupt();  // ISR (for interrupt)

    private:
        void startTimeout();
        void stopTimeout();
        static void IRAM_ATTR onTimeout();

        uint8_t trig_pin;
        uint8_t echo_pin;

        volatile unsigned long echo_start_time;
        volatile unsigned long duration;
        volatile bool data_ready;
        volatile bool timeout_occurred;
        volatile bool wait_for_echo;
        volatile bool echo_started;

        hw_timer_t* timeout_timer;

        static EchoSensor* instance;
        static const unsigned long TIMEOUT_US = 24000UL; // ~4 meters roundtrip
};
