#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include "TimeOfFlightSensor.h"
#include <vector>

// Vector to hold multiple TOF sensor objects
std::vector<TimeOfFlightSensor> tofSensors;
uint8_t tofSensorPins[] = {3, 4, 5}; // Example pins for TOF sensors'

void setup() {
    Serial.begin(9600);
    Serial.println("In Setup function");

    // Initialize multiple TOF sensors
    const int numSensors = 3; // Change this value to set the number of sensors
    for (int i = 0; i < numSensors; ++i) {
        tofSensors.emplace_back(i + 1, tofSensorPins[i]); // Example: Assign unique IDs and pins
    }

    // Create tasks
    // xTaskCreate(TaskFunction, Name, StackDepth, Parameters, Priority, TaskHandle)
    xTaskCreate(TaskReadTOF, "ReadTOF", 100, NULL, 1, NULL);
    xTaskCreate(TaskSendData, "SendData", 100, NULL, 2, NULL);
    xTaskCreate(TaskProcessData, "ProcessData", 100, NULL, 3, NULL);
    xTaskCreate(TaskActuation, "Actuation", 100, NULL, 4, NULL);
}

void loop() {
    // Empty loop as tasks handle the functionality
}

static void TaskReadTOF(void* pvParameters) {
    // pvParameters is a pointer to the parameters passed during task creation.
    // It can be used to pass data to the task, but here it is unused (NULL).
    while (1) {
        for (const auto& sensor : tofSensors) {
            int tofReading = sensor.read();
            Serial.print("TOF Sensor ");
            Serial.print(sensor.getId()); // Assuming getId() returns the sensor ID
            Serial.print(" Reading: ");
            Serial.println(tofReading);
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

static void TaskSendData(void* pvParameters) {
    while (1) {
        Serial.println("Sending data...");
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}

static void TaskProcessData(void* pvParameters) {
    while (1) {
        Serial.println("Processing data...");
        vTaskDelay(300 / portTICK_PERIOD_MS);
    }
}

static void TaskActuation(void* pvParameters) {
    while (1) {
        Serial.println("Actuating...");
        vTaskDelay(400 / portTICK_PERIOD_MS);
    }
}