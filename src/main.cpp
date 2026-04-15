// required pins: 10 (2x2 I2C, 2 for echosensor, 2 for speakers, 2 for vibration motors), ESP32-S3 supermini has these pins marked as safe to use: IO1, IO2, IO4, IO5, IO6, IO7, IO8, IO15, IO16, NO I2C/PWM but safe: IO17, IO18, IO21
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <vl53l7cx_class.h>  // Latest STM32duino header
#include <vector>
#include <array>

#include "system_parameters.h"
#include "tof.h"
#include "echo_sensor.h"

// TOF sensors
SensorSize sensorSize = SIZE_4X4; //SIZE_8X8;
uint8_t numberOfSensors = 2;

#define LPn_PIN_1 2
#define SDA_PIN_1 4
#define SCL_PIN_1 3
ToFSensor sensor1(&Wire, LPn_PIN_1, sensorSize, 1);

#define LPn_PIN_2 7
#define SDA_PIN_2 6
#define SCL_PIN_2 5
ToFSensor sensor2(&Wire1, LPn_PIN_2, sensorSize, 2);

// data buffer
uint8_t bufferIndex = 0;
uint8_t bufferLength = 12;
// list of lists. (dynamic size) init directly based on size, and combinedframe resolution
std::vector<std::vector<uint16_t>> rawDataBuffer(
    bufferLength, std::vector<uint16_t>(sensorSize * numberOfSensors)
);

// LED: setup built in NeoPixel
#define NEOPIXEL_PIN 48  // 21 for old ESP32
#define NUM_PIXELS 1
Adafruit_NeoPixel pixels(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// Echosensor setup: trigpin, echopin
EchoSensor echosensor(8, 1); // 8, 1
uint16_t echoDistance = 0;
unsigned long echototal = 0;
uint8_t echocount = 0;

// Feedback
#define LEFT_SPEAKER_PIN 10
#define RIGHT_SPEAKER_PIN 9
#define LEFT_VIBRATION_PIN 13
#define RIGHT_VIBRATION_PIN 11

#define PWM_RESOLUTION 8
// duty= 200
// min = 200 - 5000Hz frequency range

uint8_t storedLeftIntensity = 0;
uint8_t storedMidIntensity = 0;
uint8_t storedRightIntensity = 0;

unsigned long deltaTime = 0;
unsigned long lastFeedbackUpdate = 0;
const unsigned long feedbackUpdateInterval = 1000; // milliseconds
const unsigned long feedbackDurationInterval = 100; // milliseconds
bool feedbackIsOn = false;


// void giveUserFeedback(uint8_t leftIntensity, uint8_t middleIntensity, uint8_t rightIntensity);

void setupFeedback(){
    // channel: 0 = speaker left, 1 = speaker right, 2 = vibration left, 3 = vibration right

    pinMode(LEFT_SPEAKER_PIN, OUTPUT);
    pinMode(RIGHT_SPEAKER_PIN, OUTPUT);
    pinMode(LEFT_VIBRATION_PIN, OUTPUT);
    pinMode(RIGHT_VIBRATION_PIN, OUTPUT);

    ledcSetup(0, 1000, 8);
    ledcAttachPin(LEFT_SPEAKER_PIN, 0);
    ledcWrite(0, 0);

    ledcSetup(1, 1000, 8);
    ledcAttachPin(RIGHT_SPEAKER_PIN, 1);
    ledcWrite(1, 0);

    // right vibration setup
    ledcSetup(2, 1000, 8);
    ledcAttachPin(RIGHT_VIBRATION_PIN, 2);
    ledcWrite(2, 0);

    // left vibration setup
    ledcSetup(3, 1000, 8);
    ledcAttachPin(LEFT_VIBRATION_PIN, 3);
    ledcWrite(3, 0);

}

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

void setupTOF(){
    // init TOF sensors
    // I2C: sda=5, scl=6
    Wire.begin(SDA_PIN_1, SCL_PIN_1);
    Wire.setClock(800000);  // 800kHz
    Wire.setTimeout(100);   // 100ms timeout

    // I2C: sda=3, scl=2
    Wire1.begin(SDA_PIN_2, SCL_PIN_2);
    Wire1.setClock(800000);  // 800kHz
    Wire1.setTimeout(100);   // 100ms timeout

    //start sensors, if fail -> Red
    if(!sensor1.begin(sensorSize, 45, 20, 50)) {
        pixels.setPixelColor(0, pixels.Color(255, 0, 0));
        pixels.show();
        delay(2000);
        esp_restart();
    }
    if(!sensor2.begin(sensorSize, 45, 20, 50)) {
        pixels.setPixelColor(0, pixels.Color(255, 0, 0));
        pixels.show();
        delay(2000);
        esp_restart();
    }
}

void setup() {
    // serial setup
    Serial.begin(115200);
    delay(3000);
    Serial.println("ESP32-S3 + VL53L7CX v1.0.3 + NeoPixel");
    //while(!Serial);

    setupFeedback();
    setupLed();
    setupTOF();

    pixels.setPixelColor(0, pixels.Color(0, 255, 0));
    pixels.show();
}

uint8_t distanceToIntensity(uint16_t distance) {
    if (distance == 0) return 0;
    return map(distance, 0, 1000, 100, 0); // linear mapping from 0-1000mm to 100-0 intensity
}

void detectCloseObject(
    const std::vector<uint16_t> &grid,
    uint8_t &storedLeftIntensity,
    uint8_t &storedMidIntensity,
    uint8_t &storedRightIntensity,
    int threshold,
    float percentage
) {
    uint8_t totalCols = 8;
    uint8_t totalRows = 4;

    struct Zone {
        const char* label;
        uint8_t startCol;
        uint8_t endCol;
    };

    Zone zones[] = {
        { "LEFT",   0, 2 },
        { "MIDDLE", 2, 6 },
        { "RIGHT",  6, 8 }
    };

    
    // Serial.println();
    for (const Zone &zone : zones) {
        int count = 0;
        int total = 0;
        uint16_t minDist = 9999;

        for (uint8_t row = 0; row < totalRows; ++row) {
            for (uint8_t col = zone.startCol; col < zone.endCol; ++col) {
                uint16_t val = grid[row * totalCols + col];

                if (val == 0) continue;
                if (val < minDist) minDist = val;
                total++;
                if (val < threshold) count++;
            }
        }

        if (total == 0) continue;

        if (count >= total * percentage) {
            uint8_t intensity = distanceToIntensity(minDist);

            if (strcmp(zone.label, "LEFT") == 0) {
                storedLeftIntensity = intensity;
            } 
            else if (strcmp(zone.label, "RIGHT") == 0) {
                storedRightIntensity = intensity;
            } 
            else {
                storedMidIntensity = intensity;
            }

            // Serial.print(zone.label);
            // Serial.print(": = ");
            // Serial.print(intensity);
            // Serial.print(", ");
        }
    }
}

void dumpDataFrame(const std::vector<uint16_t> &data) {
    // utility function to print the data frame in a readable format for debugging.
    Serial.println("Data Frame:");
    uint8_t cols = (sensorSize == SIZE_8X8) ? 8 : 4;
    uint8_t rowLength = cols * numberOfSensors;

    for (size_t i = 0; i < data.size(); ++i) {
        Serial.print(data[i]);
        Serial.print(", ");
        if ((i + 1) % rowLength == 0) {
            Serial.println();
        }
    }
}

void combineGrid(const std::vector<uint16_t> &grid1, const std::vector<uint16_t> &grid2, std::vector<uint16_t> &combined) {
    // combine two smaller grids into one larger grid. (e.g. 4x4 + 4x4 -> 8x4)
    uint8_t cols = (sensorSize == SIZE_8X8) ? 8 : 4;
    uint8_t rows = sensorSize / cols;

    if (grid1.size() != sensorSize || grid2.size() != sensorSize) return;
    if (combined.size() != sensorSize * numberOfSensors) return;

    for (uint8_t row = 0; row < rows; ++row) {
        for (uint8_t col = 0; col < cols; ++col) {
            combined[row * cols * numberOfSensors + col] = grid1[row * cols + col];
        }

        for (uint8_t col = 0; col < cols; ++col) {
            combined[row * cols * numberOfSensors + cols + col] = grid2[row * cols + col];
        }
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

void mapIntensityToDuty(uint8_t &left, uint8_t &right, uint8_t leftintensity, uint8_t midintensity, uint8_t rightintensity) {
    // filter out to low values
    if(leftintensity <= 0 && midintensity <= 0 && rightintensity <= 0) {
        left = 0;
        right = 0;
        return;
    }

    // convert intensity percentage to duty cycle (0-255)
    leftintensity = map(leftintensity, 0, 100, 50, 255);
    midintensity = map(midintensity, 0, 100, 50, 255);
    rightintensity = map(rightintensity, 0, 100, 50, 255);

    uint8_t maxIntensity = max(leftintensity, max(midintensity, rightintensity));

    if(maxIntensity == leftintensity){
        // it is left
        left = leftintensity;
        right = 0;
        Serial.println("LEFT");
        Serial.print(leftintensity);
        return;
    } else if (maxIntensity == rightintensity){
        // it is right
        left = 0;
        right = rightintensity;
        Serial.println("RIGHT");
        Serial.print(rightintensity);
        return;
    } else {
        // it was the middle
        left = midintensity;
        right = midintensity;
        Serial.println("MIDDLE");
        Serial.print(midintensity);
        return;
    }
}

void writeFeedback(uint8_t left, uint8_t right){
    // write to pwm channels
    // speakers
    // ledcWrite(0, left);
    // ledcWrite(1, right);

    // vibration
    ledcWrite(2, left);
    ledcWrite(3, right);
}

void loop() {
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

        // Serial.println("Averaged Grid, with depth " + String(depth) + ":");
        // dumpDataFrame(averagedGrid);
        
        detectCloseObject(averagedGrid, storedLeftIntensity, storedMidIntensity, storedRightIntensity, 1000, 0.70);
    }
      
    // read echo sensor data (non-blocking)
    if(echosensor.newDataAvailable()) {
        if (echosensor.timedOut()) {
            Serial.println("Echo Sensor: No object detected within range.");
        } else {
            echoDistance = echosensor.getDistanceCM();
            echocount++;
            echototal = echototal + echoDistance;
        }
        echosensor.trigger();
    }

    // time since last feedback update
    deltaTime = millis() - lastFeedbackUpdate;

    // write feedback value every 1sec
    if (deltaTime >= feedbackUpdateInterval || lastFeedbackUpdate > millis()) {     // also account for millis() overflow
        lastFeedbackUpdate = millis();
        feedbackIsOn = true;

        Serial.print("Update feedback values");
        
        uint8_t leftDuty, rightDuty;
        mapIntensityToDuty(leftDuty, rightDuty, storedLeftIntensity, storedMidIntensity, storedRightIntensity);
        Serial.print("Left Duty: ");
        Serial.print(leftDuty); 
        Serial.print(", Right Duty: ");
        Serial.println(rightDuty);
        
        writeFeedback(leftDuty, rightDuty);

        // reset intensities after feedback is given
        storedLeftIntensity = 0;
        storedMidIntensity = 0;
        storedRightIntensity = 0;
    }

    // stop feedback after shorter duration
    if(deltaTime > feedbackDurationInterval && feedbackIsOn || lastFeedbackUpdate > millis()) {     // also account for millis() overflow
        // set left and right duty to zero to stop feedback
        Serial.println("Stop feedback");
        writeFeedback(0, 0);
        feedbackIsOn = false;
    }

    delay(10); // Slow down the loop to prevent excessive polling
}

