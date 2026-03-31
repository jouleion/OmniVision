// implementation of Echo sensor object.
#include "echo_sensor.h"

EchoSensor* EchoSensor::instance = nullptr;

EchoSensor::EchoSensor(uint8_t trig_pin, uint8_t echo_pin) :
    trig_pin(trig_pin), echo_pin(echo_pin), echo_start_time(0), duration(0), data_ready(false), timeout_occurred(false), wait_for_echo(false), echo_started(false), timeout_timer(nullptr)
{
}

bool EchoSensor::begin() {
    // Initialize pins for the echo sensor
    pinMode(trig_pin, OUTPUT);
    pinMode(echo_pin, INPUT);
    digitalWrite(trig_pin, LOW);
    instance = this;                                    // Set the static instance pointer to this object

    timeout_timer = timerBegin(0, 80, true);            // 1us tick on ESP32
    timerAttachInterrupt(timeout_timer, &EchoSensor::onTimeout, true);
    timerAlarmWrite(timeout_timer, TIMEOUT_US, false);  // single-shot timeout at ~4 meters
    timerAlarmDisable(timeout_timer);

    attachInterrupt(digitalPinToInterrupt(echo_pin), handleInterrupt, CHANGE); // Attach interrupt to the echo pin, triggering on change (rising or falling edge)
    return true;
}

void EchoSensor::startTimeout() {
    timeout_occurred = false;
    timerWrite(timeout_timer, 0);
    timerAlarmEnable(timeout_timer);
}

void EchoSensor::stopTimeout() {
    timerAlarmDisable(timeout_timer);
}

void IRAM_ATTR EchoSensor::onTimeout() {
    if (!instance) return;
    if (!instance->data_ready) {
        instance->duration = TIMEOUT_US;
        instance->timeout_occurred = true;
        instance->data_ready = true;
        instance->wait_for_echo = false;
        instance->echo_started = false;
    }
    timerAlarmDisable(instance->timeout_timer);
}

void EchoSensor::trigger() {
    data_ready = false;                                 // reset data ready flag
    timeout_occurred = false;
    wait_for_echo = true;
    echo_started = false;
    echo_start_time = 0;

    digitalWrite(trig_pin, LOW);                        // ensure the trig_pin is at 0 before sending ping
    delayMicroseconds(2);
    digitalWrite(trig_pin, HIGH);                       // set trig_pin to high to send ultrasonic sound
    delayMicroseconds(10);                              // make sound 10 us
    digitalWrite(trig_pin, LOW);                        // set trig_pin back to low
    startTimeout();                                     // start 4m timeout timer
}

uint16_t EchoSensor::getDistanceCM() {
    if (data_ready) {
        if (timeout_occurred) {
            return 400;                                // timeout indicates object was not detected within 4 meters
        }
        uint16_t distance_cm = duration * 0.017145;     // speed of sound would result into 0.03429 (340.29 m/s -> 0.03429 cm/us), sound has to travel twice the distance (travel to wall, bounce and return to the sensor), so the value is divided by 2
        return distance_cm;
    }
    return 0;                                           // return 0 to indicate that no valid measurement is available
}

bool EchoSensor::newDataAvailable() {
    return data_ready;
}

bool EchoSensor::timedOut() {
    return timeout_occurred;
}

void EchoSensor::handleInterrupt(){
    if (!instance) return;                              // check if a measurement was started or not (if not, it might be random noise changing the input value of the pin)

    int level = digitalRead(instance->echo_pin);
    if (!instance->wait_for_echo) return;

    if (level == HIGH) {
        if (!instance->echo_started) {
            instance->echo_started = true;
            instance->echo_start_time = micros();
        }
    }
    else { // LOW
        if (instance->echo_started && !instance->data_ready) {
            unsigned long pulse = micros() - instance->echo_start_time;
            if (pulse >= 100 && pulse <= EchoSensor::TIMEOUT_US) {
                instance->duration = pulse;
                instance->data_ready = true;
                instance->timeout_occurred = false;
                instance->wait_for_echo = false;
                instance->stopTimeout();
            }
        }
    }
}