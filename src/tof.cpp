// implementation of ToF class functions.
#include "tof.h"

ToFSensor::ToFSensor(TwoWire *wire, uint8_t lpn_pin, SensorSize size, uint8_t sensorId) :
    sensor(wire, lpn_pin),
    sensorId(sensorId)
{
    // use sensor size as the length of the rad data element.
    rawDataGrid.resize(size);
}

bool ToFSensor::begin(SensorSize newSize, uint8_t frequency = 60, uint8_t integrationTime = 5, uint8_t sharpenerPercent = 80) {
    // manual override
    size = newSize;
    rawDataGrid.resize(size);

    // start sensor and check status
    sensor.begin();
    int init_status = sensor.init_sensor();
    if (init_status != VL53L7CX_STATUS_OK) {
        Serial.print("sensor"); Serial.print(sensorId); Serial.print(" (Error: "); Serial.print(init_status); Serial.println(")");
        return false;
    }

    // Set correct resolution
    sensor.vl53l7cx_set_resolution((size == SIZE_8X8) ? VL53L7CX_RESOLUTION_8X8 : VL53L7CX_RESOLUTION_4X4);

    // Set frequency
    if(frequency > 60) frequency = 60;
    if(frequency < 1) frequency = 1;
    sensor.vl53l7cx_set_ranging_frequency_hz(frequency);

    // Short integration time for speed
    sensor.vl53l7cx_set_integration_time_ms(integrationTime);  // 5ms = fast

    // Increase sensitivity
    if(sharpenerPercent > 100) sharpenerPercent = 100;
    if(sharpenerPercent < 0) sharpenerPercent = 0;
    sensor.vl53l7cx_set_sharpener_percent(sharpenerPercent);  // More zones trigger

    // Start ranging as a test
    int start_status = sensor.vl53l7cx_start_ranging();
    if (start_status != VL53L7CX_STATUS_OK) {
        Serial.println("sensor"); Serial.print(sensorId); Serial.println(" start ranging FAILED");
        return false;
    } else {
        Serial.println("sensor"); Serial.print(sensorId); Serial.println(" READY");
        return true;
    }
}


void ToFSensor::setSize(SensorSize newSize) {
    size = newSize;
    rawDataGrid.resize(size);
    sensor.vl53l7cx_set_resolution((size == SIZE_8X8) ? VL53L7CX_RESOLUTION_8X8 : VL53L7CX_RESOLUTION_4X4);
}

void ToFSensor::setFrequency(uint8_t frequency) {
    if(frequency > 60) frequency = 60;
    if(frequency < 1) frequency = 1;
    sensor.vl53l7cx_set_ranging_frequency_hz(frequency);
}

bool ToFSensor::getSensorReady() {
    uint8_t new_data_ready = 0;
    sensor.vl53l7cx_check_data_ready(&new_data_ready);
    if (new_data_ready) {
        VL53L7CX_ResultsData results;
        int status = sensor.vl53l7cx_get_ranging_data(&results);
        if (status == VL53L7CX_STATUS_OK) {
            for (size_t i = 0; i < rawDataGrid.size(); ++i) {
                rawDataGrid[i] = results.distance_mm[i];
            }
            return true;
        }
    }
    return false;
}

std::vector<uint16_t> ToFSensor::fetchRawData() {
    return rawDataGrid;
}
