#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <vl53l7cx_class.h>  // Latest STM32duino header
#include <vector>
#include <array>
#include "tof.h"

// setup built in NeoPixel
#define NEOPIXEL_PIN 21
#define NUM_PIXELS 1
Adafruit_NeoPixel pixels(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// setup two I2C busses.
#define LPn_PIN_1 4
#define SDA_PIN_1 5
#define SCL_PIN_1 6
// VL53L7CX sensor1(&Wire, LPn_PIN_1);
// Construct ToFSensor as: (TwoWire*, lpn_pin, size, sensorId)
ToFSensor sensor1(&Wire, LPn_PIN_1, SIZE_8X8, 1);

#define LPn_PIN_2 7
#define SDA_PIN_2 3
#define SCL_PIN_2 2
// VL53L7CX sensor2(&Wire1, LPn_PIN_2);
ToFSensor sensor2(&Wire1, LPn_PIN_2, SIZE_8X8, 2);

// funky led functions.
unsigned long lastBlink = 0;
void nonBlockingBlinkError();
// void readToFSensor(VL53L7CX &sensor, uint8_t sensorId);

// Sensor data arrays for object detection.
uint16_t combinedGrid[128];
uint16_t sensor1Data[64];
uint16_t sensor2Data[64];
bool sensor1Ready = false;
bool sensor2Ready = false;

// larger buffer
uint8_t bufferSize = 10; // 10 frames of 128.
// a vector with an array in it of 128 values. the vector can be any length (dynamic).
std::vector<std::array<uint16_t, 128>> buffer(bufferSize);

void setup() {
    // serial setup
    Serial.begin(115200);
    delay(3000);
    Serial.println("ESP32-S3 + VL53L7CX v1.0.3 + NeoPixel");
    while(!Serial);

    // NeoPixel setup
    pixels.begin();
    pixels.clear();
    pixels.show();

    // Wake up lights
    pixels.setPixelColor(0, pixels.Color(0, 0, 255));  // Blue
    pixels.show();
    delay(100);
    pixels.clear();
    pixels.show();
    delay(100);
    pixels.setPixelColor(0, pixels.Color(0, 0, 255));  // Blue
    pixels.show();

    // I2C: sda=5, scl=6
    Wire.begin(SDA_PIN_1, SCL_PIN_1);
    Wire.setClock(800000);  // 800kHz

    // I2C: sda=3, scl=2
    Wire1.begin(SDA_PIN_2, SCL_PIN_2);
    Wire1.setClock(800000);  // 800kHz

    // start sensors, if fail -> Red
    if(!sensor1.begin(SIZE_8X8, 60, 5, 80)) {
        pixels.setPixelColor(0, pixels.Color(255, 0, 0));
        pixels.show();
        delay(5000);
        esp_restart();
    }
    if(!sensor2.begin(SIZE_8X8, 60, 5, 80)) {
        pixels.setPixelColor(0, pixels.Color(255, 0, 0));
        pixels.show();
        delay(5000);
        esp_restart();
    }

    pixels.setPixelColor(0, pixels.Color(0, 255, 0));  // Green
    pixels.show();

    // VL53L7CX init sensor 1
    // Serial.print("VL53L7CX 1 init...");
    // sensor1.begin();
    // int init_status = sensor1.init_sensor();

    // if (init_status != VL53L7CX_STATUS_OK) {
    //     Serial.print("FAILED 1 (status: "); Serial.print(init_status); Serial.println(")");
    //     pixels.setPixelColor(0, pixels.Color(255, 0, 0));  // Red ERROR
    //     pixels.show();
    //     while(1) { nonBlockingBlinkError(); }
    // }

    // pixels.setPixelColor(0, pixels.Color(255, 165, 0));  // Orange
    // pixels.show();
    // delay(100);

    // // 1. Set 8x8 RESOLUTION (not default 4x4!)
    // sensor1.vl53l7cx_set_resolution(VL53L7CX_RESOLUTION_8X8);

    // // 2. Set 60Hz frequency (max speed)
    // sensor1.vl53l7cx_set_ranging_frequency_hz(60);

    // // 3. Short integration time for speed
    // sensor1.vl53l7cx_set_integration_time_ms(5);  // 5ms = fast

    // // 4. Increase sensitivity
    // sensor1.vl53l7cx_set_sharpener_percent(80);  // More zones trigger

    // pixels.setPixelColor(0, pixels.Color(255, 255, 0));  // Yellow for config done
    // pixels.show();

    // // Start ranging (continuous mode, 8x8 zones)
    // int start_status = sensor1.vl53l7cx_start_ranging();
    // if (start_status != VL53L7CX_STATUS_OK) {
    //     Serial.println("Start ranging 1 FAILED");
    // } else {
    //     Serial.println("VL53L7CX 1 READY - 8x8 zones active");
    //     pixels.setPixelColor(0, pixels.Color(0, 255, 0));  // Green OK
    //     pixels.show();
    //     delay(100);
    // }

    // pixels.setPixelColor(0, pixels.Color(100, 0, 255));  // Blue for next sensor
    // pixels.show();
    // delay(100);

    // sensor 2
    // VL53L7CX init sensor 2
    // Serial.print("VL53L7CX 2 init...");
    // sensor2.begin();
    // init_status = sensor2.init_sensor();

    // if (init_status != VL53L7CX_STATUS_OK) {
    //     Serial.print("FAILED 2 (status: "); Serial.print(init_status); Serial.println(")");
    //     pixels.setPixelColor(0, pixels.Color(255, 0, 0));  // Red ERROR
    //     pixels.show();
    //     while(1) { nonBlockingBlinkError(); }
    // }

    // pixels.setPixelColor(0, pixels.Color(255, 165, 0));  // Orange
    // pixels.show();
    // delay(100);

    // // 1. Set 8x8 RESOLUTION (not default 4x4!)
    // sensor2.vl53l7cx_set_resolution(VL53L7CX_RESOLUTION_8X8);

    // // 2. Set 60Hz frequency (max speed)
    // sensor2.vl53l7cx_set_ranging_frequency_hz(60);

    // // 3. Short integration time for speed
    // sensor2.vl53l7cx_set_integration_time_ms(5);  // 5ms = fast

    // // 4. Increase sensitivity
    // sensor2.vl53l7cx_set_sharpener_percent(80);  // More zones trigger

    // pixels.setPixelColor(0, pixels.Color(255, 255, 0));  // Yellow for config done
    // pixels.show();

    // // Start ranging (continuous mode, 8x8 zones)
    // start_status = sensor2.vl53l7cx_start_ranging();
    // if (start_status != VL53L7CX_STATUS_OK) {
    //     Serial.println("Start ranging 2 FAILED");
    // } else {
    //     Serial.println("VL53L7CX 2 READY - 8x8 zones active");
    //     pixels.setPixelColor(0, pixels.Color(0, 255, 0));  // Green OK
    //     pixels.show();
    // }
}

// DETECTION CODE------------------------------------------------------------------------------------------------
void detectCloseObject(uint16_t *grid, int startCol, int endCol, const char* label, int threshold = 1000, float percentage = 0.35) {
    int count = 0;
    int total = 0;

    for (int row = 0; row < 8; row++) {
        for (int col = startCol; col < endCol; col++) {
            int idx = row * 16 + col;
            uint16_t val = grid[idx];

            if (val == 0) continue;

            total++;
            if (val < threshold) count++;
        }
    }

    if (total == 0) return;

    if (count >= total * percentage) {
        Serial.print(label);
        Serial.println(": OBJECT DETECTED");
    }
}
// -------------------------------------------------------------------------------------------------------------


void loop() {
    // try to do a measurement
    bool sensor1Ready = sensor1.getSensorReady();
    bool sensor2Ready = sensor2.getSensorReady();

    // if sensor is ready, get the new data.
    if(sensor1Ready){
        std::vector<uint16_t> data1 = sensor1.fetchRawData();
    }

    if(sensor2Ready){
        std::vector<uint16_t> data2 = sensor2.fetchRawData();
    }


    // DETECTION CODE------------------------------------------------
    if (sensor1Ready && sensor2Ready) {
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                combinedGrid[row * 16 + col] = sensor1Data[row * 8 + col];
                combinedGrid[row * 16 + col + 8] = sensor2Data[row * 8 + col];
            }
        }
        // add combined grid to the buffer.
        // addToRawBuffer();

        // filter: LPF
        // timeAverage(buffer);
        

        

        detectCloseObject(combinedGrid, 0, 5, "LEFT");
        detectCloseObject(combinedGrid, 5, 11, "MIDDLE");
        detectCloseObject(combinedGrid, 11, 16, "RIGHT");
    }
    //----------------------------------------------------------------
}


// void readToFSensor(VL53L7CX &sensor, uint8_t sensorId) {
//     uint8_t new_data_ready = 0;
//     sensor.vl53l7cx_check_data_ready(&new_data_ready);

//     if (new_data_ready) {
//         VL53L7CX_ResultsData results;
//         int status = sensor.vl53l7cx_get_ranging_data(&results);
        
        // if (status == VL53L7CX_STATUS_OK) {
        //     Serial.print("Sensor "); 
        //     Serial.print(sensorId); 
        //     Serial.print(": ");
        //     Serial.println("\n8x8 GRID");
            
            // DETECTION CODE----------------------------------
            // if (sensorId == 1) {
            //     for (int i = 0; i < 64; i++) {
            //         sensor1Data[i] = results.distance_mm[i];
            //     }
            //     sensor1Ready = true;
            // }

            // if (sensorId == 2) {
            //     for (int i = 0; i < 64; i++) {
            //         sensor2Data[i] = results.distance_mm[i];
            //     }
            //     sensor2Ready = true;
            // }
            //--------------------------------------------------
            
            // // Print 8x8 distance grid (64 zones total)
            // for (int row = 0; row < 8; row++) {
            //     for (int col = 0; col < 8; col++) {
            //         int zone = row * 8 + col;
            //         uint16_t distance = results.distance_mm[zone];
                    
            //         // Print distance (pad to 4 chars)
            //         if (distance < 1000) Serial.print(" ");
            //         if (distance < 100) Serial.print(" ");
            //         if (distance < 10) Serial.print(" ");
                    
            //         Serial.print(distance);
            //         Serial.print(" ");
            //     }
            //     Serial.println();  // New line after row
            // }
//         }
//     }
// }

void timeAverage(std::vector<std::array<uint16_t, 128>> &buffer) {
    // do a low pass filter on each data value over time. so i=1 for buffer[0][i], buffer[1][i], buffer[2][i]... up to buffer[n][i]. average them and store in averagedGrid[i].
    // std::array<uint16_t, 128> averagedGrid;
    // for (int i = 0; i < 128; i++) {
    //     uint32_t sum = 0;
    //     int count = 0;
    //     for (const auto& frame : buffer) {
    //         if (frame[i] > 0) { // only consider valid readings
    //             sum += frame[i];
    //             count++;
    //         }
    //     }
    //     averagedGrid[i] = (count > 0) ? (sum / count) : 0; // average or zero if no valid data
    // }

}

// void addToRawBuffer(){
//     // Add new frame to buffer.
//     std::array<uint16_t, 128> newFrame;
//     // Use a circular overwrite when full to avoid expensive shifts; buffer overwrites oldest frame circularly when full.
//     std::copy(std::begin(combinedGrid), std::end(combinedGrid), std::begin(newFrame));

//     if (buffer.size() < bufferSize) {
//         // still space: append
//         buffer.push_back(newFrame);
//     } else {
//         // buffer full: overwrite oldest entry in-place using a rotating index
//         static size_t insertPos = 0; // index of next slot to overwrite (persists)
//         buffer[insertPos] = newFrame; // store new frame into oldest position
//         insertPos = (insertPos + 1) % bufferSize; // advance and wrap index
//     }
// }

// void nonBlockingBlinkError() {
//     if (millis() - lastBlink > 300) {
//         static bool ledState = false;
//         if (ledState) {
//         pixels.setPixelColor(0, pixels.Color(255, 0, 0));
//         } else {
//         pixels.clear();
//         }
//         pixels.show();
//         ledState = !ledState;
//         lastBlink = millis();
//     }
// }
