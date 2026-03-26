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
ToFSensor sensor1(&Wire, LPn_PIN_1, SIZE_8X8, 1);

#define LPn_PIN_2 7
#define SDA_PIN_2 3
#define SCL_PIN_2 2
ToFSensor sensor2(&Wire1, LPn_PIN_2, SIZE_8X8, 2);

// funky led functions.
unsigned long lastBlink = 0;
void nonBlockingBlinkError();

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
        // print data for debugging
        Serial.println("Sensor 1:");
        
        dumpDataFrame(data1);

        sensor1.setFrequency(30);
    }

    if(sensor2Ready){
        std::vector<uint16_t> data2 = sensor2.fetchRawData();
        // print data for debugging
        Serial.println("Sensor 2:");
        // if size == 64, print as 8x8 grid. if size == 16, print as 4x4 grid.
        dumpDataFrame(data2);
        sensor2.setFrequency(30);
    }


    // DETECTION CODE------------------------------------------------
    if (sensor1Ready && sensor2Ready) {


        // for (int row = 0; row < 8; row++) {
        //     for (int col = 0; col < 8; col++) {
                
        //         combinedGrid[row * 16 + col] = sensor1Data[row * 8 + col];
        //         combinedGrid[row * 16 + col + 8] = sensor2Data[row * 8 + col];
        //     }
        // }


        sensor2.setFrequency(30);

        // add combined grid to the buffer.
        // addToRawBuffer();

        // filter: LPF
        // timeAverage(buffer);
        

        

        // detectCloseObject(combinedGrid, 0, 5, "LEFT");
        // detectCloseObject(combinedGrid, 5, 11, "MIDDLE");
        // detectCloseObject(combinedGrid, 11, 16, "RIGHT");
    }
    //----------------------------------------------------------------
}

void dumpDataFrame(std::vector<uint16_t> &data) {
    Serial.println("Data Frame:");
    for (size_t i = 0; i < data.size(); ++i) {
        Serial.print(data[i]);
        Serial.print(" ");
        if ((i + 1) % 8 == 0) {
            Serial.println();
        }
    }
}

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

void addToRawBuffer(){
    // Add new frame to buffer.
    std::array<uint16_t, 128> newFrame;
    // Use a circular overwrite when full to avoid expensive shifts; buffer overwrites oldest frame circularly when full.
    std::copy(std::begin(combinedGrid), std::end(combinedGrid), std::begin(newFrame));

    if (buffer.size() < bufferSize) {
        // still space: append
        buffer.push_back(newFrame);
    } else {
        // buffer full: overwrite oldest entry in-place using a rotating index
        static size_t insertPos = 0; // index of next slot to overwrite (persists)
        buffer[insertPos] = newFrame; // store new frame into oldest position
        insertPos = (insertPos + 1) % bufferSize; // advance and wrap index
    }
}