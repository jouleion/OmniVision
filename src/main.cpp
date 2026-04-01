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

// setup built in NeoPixel
#define NEOPIXEL_PIN 21
#define NUM_PIXELS 1
Adafruit_NeoPixel pixels(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

SensorSize sensorSize = SIZE_4X4; //SIZE_8X8;
uint8_t numberOfSensors = 2;

// for all feedback
static hw_timer_t *feedback_timer = nullptr;
void IRAM_ATTR onFeedbackTimerISR();
void startFeedbackTimer();
void stopFeedback();

// For speaker output
#define LEFT_SPEAKER_PIN 40                                 // Pin 40 is used for JTAG, so JTAG will not work
#define RIGHT_SPEAKER_PIN 42                                // Pin 42 seems to be free and supports PWM

#define LEFT_SPEAKER_PWM_CHANNEL 0                          // LEDC channel 0 is used for controlling the speaker tone, not for leds
#define RIGHT_SPEAKER_PWM_CHANNEL 1                         // LEDC channel 1 is used for controlling the right speaker tone
#define LEFT_SPEAKER_PWM_RESOLUTION 8
#define RIGHT_SPEAKER_PWM_RESOLUTION 8
#define LEFT_SPEAKER_DEFAULT_DUTY 128
#define RIGHT_SPEAKER_DEFAULT_DUTY 128
void startSpeakerTone(uint32_t leftFreq, uint32_t rightFreq);

// Echosensor setup: trigpin, echopin
EchoSensor echosensor(6,7);

// For vibration output
#define LEFT_VIBRATION_PIN 15
#define RIGHT_VIBRATION_PIN 16
#define LEFT_VIBRATION_PWM_CHANNEL 2
#define RIGHT_VIBRATION_PWM_CHANNEL 3
#define LEFT_VIBRATION_PWM_RESOLUTION 8
#define RIGHT_VIBRATION_PWM_RESOLUTION 8

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
    //while(!Serial);

    setupLed();

#ifndef DISABLE_TOF
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
#endif

#if defined(SPEAKER_OUTPUT) || defined(VIBRATION_OUTPUT)
    feedback_timer = timerBegin(1, 80, true); // use timer 1 to avoid conflict with echo sensor timer 0
    timerAttachInterrupt(feedback_timer, &onFeedbackTimerISR, true);
    timerAlarmWrite(feedback_timer, 100000, false); // 100 ms
    timerAlarmDisable(feedback_timer);
#endif

#ifdef SPEAKER_OUTPUT
    pinMode(LEFT_SPEAKER_PIN, OUTPUT);
    ledcSetup(LEFT_SPEAKER_PWM_CHANNEL, 1000, LEFT_SPEAKER_PWM_RESOLUTION);
    ledcAttachPin(LEFT_SPEAKER_PIN, LEFT_SPEAKER_PWM_CHANNEL);
    ledcWrite(LEFT_SPEAKER_PWM_CHANNEL, 0);
    pinMode(RIGHT_SPEAKER_PIN, OUTPUT);
    ledcSetup(RIGHT_SPEAKER_PWM_CHANNEL, 1000, RIGHT_SPEAKER_PWM_RESOLUTION);
    ledcAttachPin(RIGHT_SPEAKER_PIN, RIGHT_SPEAKER_PWM_CHANNEL);
    ledcWrite(RIGHT_SPEAKER_PWM_CHANNEL, 0);
#endif

#ifdef VIBRATION_OUTPUT
    pinMode(LEFT_VIBRATION_PIN, OUTPUT);
    ledcSetup(LEFT_VIBRATION_PWM_CHANNEL, 1000, LEFT_VIBRATION_PWM_RESOLUTION);
    ledcAttachPin(LEFT_VIBRATION_PIN, LEFT_VIBRATION_PWM_CHANNEL);
    ledcWrite(LEFT_VIBRATION_PWM_CHANNEL, 0);
    pinMode(RIGHT_VIBRATION_PIN, OUTPUT);
    ledcSetup(RIGHT_VIBRATION_PWM_CHANNEL, 1000, RIGHT_VIBRATION_PWM_RESOLUTION);
    ledcAttachPin(RIGHT_VIBRATION_PIN, RIGHT_VIBRATION_PWM_CHANNEL);
    ledcWrite(RIGHT_VIBRATION_PWM_CHANNEL, 0);
#endif

#ifdef USE_ECHO_SENSOR
    echosensor.begin();
    echosensor.trigger();
#endif

    pixels.setPixelColor(0, pixels.Color(0, 255, 0));  // Green
    pixels.show();
}

// DETECTION CODE------------------------------------------------------------------------------------------------
// void detectCloseObject(uint16_t *grid, int startCol, int endCol, const char* label, int threshold = 1000, float percentage = 0.35) {
//     int count = 0;
//     int total = 0;

//     for (int row = 0; row < 8; row++) {
//         for (int col = startCol; col < endCol; col++) {
//             int idx = row * 16 + col;
//             uint16_t val = grid[idx];

//             if (val == 0) continue;

//             total++;
//             if (val < threshold) count++;
//         }
//     }

//     if (total == 0) return;

//     if (count >= total * percentage) {
//         Serial.print(label);
//         Serial.println(": OBJECT DETECTED");
//     }
// }

void detectCloseObject(const std::vector<uint16_t> &grid, int threshold = 1000, float percentage = 0.40) {
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

    for (const Zone &zone : zones) {
        int count = 0;
        int total = 0;

        for (uint8_t row = 0; row < totalRows; ++row) {
            for (uint8_t col = zone.startCol; col < zone.endCol; ++col) {
                uint16_t val = grid[row * totalCols + col];

                if (val == 0) continue;

                total++;
                if (val < threshold) count++;
            }
        }

        if (total == 0) continue;

        if (count >= total * percentage) {
            Serial.print(zone.label);
            Serial.println(": OBJECT DETECTED");
        }
    }
}
// -------------------------------------------------------------------------------------------------------------

// void dumpDataFrame(const std::vector<uint16_t> &data) {
//     Serial.println("Data Frame:");
//     for (size_t i = 0; i < data.size(); ++i) {
//         Serial.print(data[i]);
//         Serial.print(", ");

//         // add newlime after correct length.
//         uint8_t rowLength = (sensorSize == SIZE_8X8) ? 8 : 4;
//         if ((i + 1) % rowLength == 0) {
//             Serial.println();
//         }
//     }
// }

void dumpDataFrame(const std::vector<uint16_t> &data) {
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

// Give user feedback based on the detected distance in each sector (left, middle, right). Values can range from 0 (no feedback) to 65535 (intense feedback).
void giveUserFeedback(uint16_t leftIntensity, uint16_t middleIntensity, uint16_t rightIntensity) {
    leftIntensity = max(leftIntensity, middleIntensity);
    rightIntensity = max(rightIntensity, middleIntensity);

    if (!leftIntensity && !rightIntensity) {
        stopFeedback();
        return;
    }

#ifdef SPEAKER_OUTPUT
    // Map intensity to an audible frequency range.
    // Higher intensity produces a higher pitched tone.
    uint32_t leftFreq = 0;
    uint32_t rightFreq = 0;
    if (leftIntensity) leftFreq = 200 + static_cast<uint32_t>(leftIntensity) * 16;
    if (rightIntensity) rightFreq = 200 + static_cast<uint32_t>(rightIntensity) * 16;
    if (leftFreq > 2500) leftFreq = 2500;
    if (rightFreq > 2500) rightFreq = 2500;
    if (leftFreq || rightFreq) {
        startSpeakerTone(leftFreq, rightFreq);
    }
#endif

#ifdef VIBRATION_OUTPUT
    uint8_t leftvibration = map(leftIntensity, 0, 65535, 0, 255);
    uint8_t rightvibration = map(rightIntensity, 0, 65535, 0, 255);
    ledcWrite(LEFT_VIBRATION_PWM_CHANNEL, leftvibration);
    ledcWrite(RIGHT_VIBRATION_PWM_CHANNEL, rightvibration);
    startFeedbackTimer();
#endif
}

#if defined(SPEAKER_OUTPUT) || defined(VIBRATION_OUTPUT)
void IRAM_ATTR onFeedbackTimerISR() {
    if (!feedback_timer) return;
    #ifdef SPEAKER_OUTPUT
    ledcWrite(LEFT_SPEAKER_PWM_CHANNEL, 0);
    ledcWrite(RIGHT_SPEAKER_PWM_CHANNEL, 0);
    #endif
    #ifdef VIBRATION_OUTPUT
    ledcWrite(LEFT_VIBRATION_PWM_CHANNEL, 0);
    ledcWrite(RIGHT_VIBRATION_PWM_CHANNEL, 0);
    #endif
    timerAlarmDisable(feedback_timer);
}

void startFeedbackTimer() {
    if (!feedback_timer) return;
    timerAlarmWrite(feedback_timer, 100000, false);              // 100000 us = 100 ms
    timerAlarmEnable(feedback_timer);
}

void stopFeedback() {
    if (!feedback_timer) return;
    #ifdef SPEAKER_OUTPUT
    ledcWrite(LEFT_SPEAKER_PWM_CHANNEL, 0);
    ledcWrite(RIGHT_SPEAKER_PWM_CHANNEL, 0);
    #endif
    #ifdef VIBRATION_OUTPUT
    ledcWrite(LEFT_VIBRATION_PWM_CHANNEL, 0);
    ledcWrite(RIGHT_VIBRATION_PWM_CHANNEL, 0);
    #endif
    timerAlarmDisable(feedback_timer);
}
#endif

#ifdef SPEAKER_OUTPUT
void startSpeakerTone(uint32_t leftFreq, uint32_t rightFreq) {
    if (!feedback_timer) return;
    if (leftFreq) {
        ledcWriteTone(LEFT_SPEAKER_PWM_CHANNEL, leftFreq);
        ledcWrite(LEFT_SPEAKER_PWM_CHANNEL, LEFT_SPEAKER_DEFAULT_DUTY);
    } else ledcWrite(LEFT_SPEAKER_PWM_CHANNEL, 0);
    if (rightFreq) {
        ledcWriteTone(RIGHT_SPEAKER_PWM_CHANNEL, rightFreq);
        ledcWrite(RIGHT_SPEAKER_PWM_CHANNEL, RIGHT_SPEAKER_DEFAULT_DUTY);
    } else ledcWrite(RIGHT_SPEAKER_PWM_CHANNEL, 0);
    startFeedbackTimer();
}
#endif

#ifndef DISABLE_TOF
// void combineGrid(const std::vector<uint16_t> &grid1, const std::vector<uint16_t> &grid2, std::vector<uint16_t> &combined) {
//     // combine two same-size grids into one contiguous grid: grid1 then grid2
//     size_t n1 = grid1.size();
//     size_t n2 = grid2.size();
//     if (combined.size() != n1 + n2) return; // caller should size combined appropriately

//     for (size_t i = 0; i < n1; ++i) {
//         // element-wise copy.
//         combined[i] = grid1[i];
//     }
//     for (size_t i = 0; i < n2; ++i) {
//         // element-wise copy.
//         combined[i + n1] = grid2[i];
//     }
// }

// CHANGED COMBINE GRID TO MAKE DATA FRAME HORIZONTAL
void combineGrid(const std::vector<uint16_t> &grid1, const std::vector<uint16_t> &grid2, std::vector<uint16_t> &combined) {
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
#endif


void loop() {
    // try to do a measurement
#ifndef DISABLE_TOF
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

        // detect close objects
        detectCloseObject(averagedGrid);
        
        // store avoidance signal somewhere
                
    }
#endif            
            
    // use distance to give user feedback (intensity left, middle right)
    // 0%, 0%, 100%
    // pass stored avoidance signal.

#ifdef USE_ECHO_SENSOR
    if(echosensor.newDataAvailable()) {
        if (echosensor.timedOut()) {
            Serial.println("Echo Sensor: No object detected within range.");
        } else {
            uint16_t distance = echosensor.getDistanceCM();
            Serial.print("Echo Sensor Distance: ");
            Serial.print(distance);
            Serial.println(" cm");
        }
        echosensor.trigger();
    }
#endif


    giveUserFeedback(0, 0, 100);

#ifdef DELAY_IN_LOOP
    delay(2000);
#endif
}

