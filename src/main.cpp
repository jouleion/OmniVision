#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <vl53l7cx_class.h>  // Latest STM32duino header
#include <vector>
#include <array>

#include "system_parameters.h"
#include "tof.h"
#include "echo_sensor.h"

// setup built in NeoPixel
#define NEOPIXEL_PIN 21
#define NUM_PIXELS 1
Adafruit_NeoPixel pixels(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

SensorSize sensorSize = SIZE_4X4; //SIZE_8X8;
uint8_t numberOfSensors = 2;

// setup two I2C busses.
#define LPn_PIN_1 4
#define SDA_PIN_1 5
#define SCL_PIN_1 6
ToFSensor sensor1(&Wire, LPn_PIN_1, sensorSize, 1);

#define LPn_PIN_2 7
#define SDA_PIN_2 3
#define SCL_PIN_2 2
ToFSensor sensor2(&Wire1, LPn_PIN_2, sensorSize, 2);

// data buffer
uint8_t bufferIndex = 0;
uint8_t bufferLength = 12;
// list of lists. (dynamic size) init directly based on size, and combinedframe resolution
std::vector<std::vector<uint16_t>> rawDataBuffer(
    bufferLength, std::vector<uint16_t>(sensorSize * numberOfSensors)
);

void setupLed(){
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
}

void setup() {
    // serial setup
    Serial.begin(115200);
    delay(3000);
    Serial.println("ESP32-S3 + VL53L7CX v1.0.3 + NeoPixel");
    while(!Serial);

    setupLed();

    // I2C: sda=5, scl=6
    Wire.begin(SDA_PIN_1, SCL_PIN_1);
    Wire.setClock(800000);  // 800kHz

    // I2C: sda=3, scl=2
    Wire1.begin(SDA_PIN_2, SCL_PIN_2);
    Wire1.setClock(800000);  // 800kHz

    // start sensors, if fail -> Red
    if(!sensor1.begin(sensorSize, 60, 5, 80)) {
        pixels.setPixelColor(0, pixels.Color(255, 0, 0));
        pixels.show();
        delay(2000);
        esp_restart();
    }
    if(!sensor2.begin(sensorSize, 60, 5, 80)) {
        pixels.setPixelColor(0, pixels.Color(255, 0, 0));
        pixels.show();
        delay(2000);
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



void dumpDataFrame(const std::vector<uint16_t> &data) {
    Serial.println("Data Frame:");
    for (size_t i = 0; i < data.size(); ++i) {
        Serial.print(data[i]);
        Serial.print(", ");

        // add newlime after correct length.
        uint8_t rowLength = (sensorSize == SIZE_8X8) ? 8 : 4;
        if ((i + 1) % rowLength == 0) {
            Serial.println();
        }
    }
}

void giveUserFeedback(uint16_t leftIntensity, uint16_t middleIntensity, uint16_t rightIntensity) {
    #ifdef SPEAKER_OUTPUT
        // for large distance, speaker only
        


        #ifdef VIBRATION_OUTPUT
            // for small distance, vibration + speaker
            // process intensity to vibration too.
        #endif
    #endif

}

void combineGrid(const std::vector<uint16_t> &grid1, const std::vector<uint16_t> &grid2, std::vector<uint16_t> &combined) {
    // combine two same-size grids into one contiguous grid: grid1 then grid2
    size_t n1 = grid1.size();
    size_t n2 = grid2.size();
    if (combined.size() != n1 + n2) return; // caller should size combined appropriately

    for (size_t i = 0; i < n1; ++i) {
        // element-wise copy.
        combined[i] = grid1[i];
    }
    for (size_t i = 0; i < n2; ++i) {
        // element-wise copy.
        combined[i + n1] = grid2[i];
    }
}

void addToRawBuffer(std::vector<uint16_t> newFrame){
    // Take frame by value and move into the circular buffer to avoid element-wise copies
    size_t expected = static_cast<size_t>(sensorSize) * numberOfSensors;
    if (newFrame.size() != expected) {
        Serial.print("addToRawBuffer: unexpected frame size ");
        Serial.print(newFrame.size());
        Serial.print(" expected ");
        Serial.println(expected);
        return;
    }

    // std:move is a cheap way to transfer ownership of the data without copying. 
    rawDataBuffer[bufferIndex] = std::move(newFrame);
    bufferIndex = (bufferIndex + 1) % bufferLength; // advance and wrap index
}

void avarageGrid(std::vector<uint16_t> &averagedGrid, uint8_t depth) {
    // average for each element across the frames. resulting in one avaraged frame.
    // depth is how many recent frames are averaged.
    uint8_t frameSize = sensorSize * numberOfSensors;

    // size check
    if (averagedGrid.size() != frameSize) {
        Serial.print("avarageGrid: unexpected averagedGrid size ");
        Serial.print(averagedGrid.size());
        Serial.print(" expected ");
        Serial.println(frameSize);
        return;
    }

    // simple moving average over the last 'depth' frames in the buffer
    for (size_t i = 0; i < frameSize; ++i) {
        uint32_t sum = 0;
        uint8_t count = 0;

        // loop over 'depth' number of recent frames.
        for (uint8_t j = 0; j < depth; ++j) {
            // determine index. (remeber that buffer is a circular buffer, we wrap around form current index.)
            size_t idx = (bufferIndex + bufferLength - 1 - j) % bufferLength;
            uint16_t val = rawDataBuffer[idx][i];

            // filter out 0 readings.
            if (val > 0) {
                sum += val;
                count++;
            }
        }

        // store this elements average value.
        averagedGrid[i] = (count > 0) ? (sum / count) : 0; // average or zero if no valid data
    }

    
}

void bruteForceTuning(){

    int frequencies[] = {15, 30, 60, 120};
    int integrationTimes[] = {1, 5, 10, 20};
    int sharpenerPercents[] = {0, 20, 50, 80};

    for (int freq : frequencies) {
        for (int intTime : integrationTimes) {
            for (int sharp : sharpenerPercents) {
                

                // reinit sensor


                // x measurements (output to serial)
            }
        }
    }

}


void loop() {

    delay(100);


    // try to do a measurement
    bool sensor1Ready = sensor1.getSensorReady();
    bool sensor2Ready = sensor2.getSensorReady();

    // if both sensors are ready, process the data.
    if (sensor1Ready && sensor2Ready) {
 
        const std::vector<uint16_t> &data1_ref = sensor1.fetchRawData();
        const std::vector<uint16_t> &data2_ref = sensor2.fetchRawData();

        // dump data frame
        // Serial.println("Sensor 1:");
        // dumpDataFrame(data1_ref);

        // Serial.println("Sensor 2:");
        // dumpDataFrame(data2_ref);

        // create larger grid
        // 128x2 or 32x2 length
        uint8_t combinedLength = sensorSize * numberOfSensors;  
        std::vector<uint16_t> combinedGrid(combinedLength);

        // double check size of data
        if (data1_ref.size() == sensorSize && data2_ref.size() == sensorSize) {
            // combine data into one grid. (pass combinedGrid by reference)
            combineGrid(data1_ref, data2_ref, combinedGrid);

            // move into the buffer to avoid copying element-by-element
            addToRawBuffer(std::move(combinedGrid));
        }

        // rolling average over all the frames -> (average Grid)
        // how many frame to average since last frame.
        const uint8_t depth = bufferLength; 
        std::vector<uint16_t> averagedGrid(combinedLength);
        avarageGrid(averagedGrid, depth);

        Serial.println("Averaged Grid, with depth " + String(depth) + ":");
        dumpDataFrame(averagedGrid);
  
        // detect the distance in each sector (left, middle, right) -> (return intensity o)
        
                // detectCloseObject(combinedGrid, 0, 5, "LEFT");
                // detectCloseObject(combinedGrid, 5, 11, "MIDDLE");
                // detectCloseObject(combinedGrid, 11, 16, "RIGHT");
        
        // store avoidance signal somewhere
                
    }
            
            
    // use distance to give user feedback (intensity left, middle right)
    // 0%, 0%, 100%
    // pass stored avoidance signal.
    giveUserFeedback(0, 0, 100);
}

