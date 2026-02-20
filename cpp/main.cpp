// OmniVision – ESP32 main sketch
// Reads sensor data and streams JSON over Serial so the Python host can consume it.

#include <Arduino.h>
#include "Sensor.h"

// ----- Configuration -----
#define SERIAL_BAUD    115200
#define SENSOR_PIN     34          // GPIO pin connected to the sensor (analog-capable)
#define SAMPLE_INTERVAL_MS 1000   // How often to sample and transmit (ms)

// ----- Global objects -----
Sensor envSensor("env_sensor", SENSOR_PIN);

void setup() {
    Serial.begin(SERIAL_BAUD);
    while (!Serial) { ; }          // Wait for Serial to be ready (needed on some boards)

    Serial.println("{\"status\":\"OmniVision ESP32 ready\"}");

    // Example: apply a calibration offset of -1.5 °C
    envSensor.setCalibrationOffset(-1.5f);
}

void loop() {
    envSensor.update();

    if (envSensor.isReady()) {
        Serial.println(envSensor.toSerialString());
    }

    delay(SAMPLE_INTERVAL_MS);
}
