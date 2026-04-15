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
#define NEOPIXEL_PIN 48  // 21 for old ESP32
#define NUM_PIXELS 1
Adafruit_NeoPixel pixels(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

SensorSize sensorSize = SIZE_4X4; //SIZE_8X8;
uint8_t numberOfSensors = 2;

// for calling the feedback function every 1 second
unsigned long previous_call = 0;
#define feedback_interval 1000 // in milliseconds

// for all feedback
static hw_timer_t *feedback_timer = nullptr;
void IRAM_ATTR onFeedbackTimerISR();
void startFeedbackTimer();
void stopFeedback();
#define feedback_duration 100000 // in microseconds
volatile bool feedbackTimerFired = false;

// For speaker output
#define LEFT_SPEAKER_PIN 10
#define RIGHT_SPEAKER_PIN 9

#define LEFT_SPEAKER_PWM_CHANNEL 0                          // LEDC channel 0 is used for controlling the speaker tone, not for leds
#define RIGHT_SPEAKER_PWM_CHANNEL 1                         // LEDC channel 1 is used for controlling the right speaker tone
#define LEFT_SPEAKER_PWM_RESOLUTION 8
#define RIGHT_SPEAKER_PWM_RESOLUTION 8
#define LEFT_SPEAKER_DEFAULT_DUTY 200
#define RIGHT_SPEAKER_DEFAULT_DUTY 200
#define MIN_FREQ 200
#define MAX_FREQ 5000
void startSpeakerTone(uint32_t leftFreq, uint32_t rightFreq);

// Echosensor setup: trigpin, echopin
EchoSensor echosensor(8, 1); // 8, 1
uint16_t echoDistance = 0;
unsigned long echototal = 0;
uint8_t echocount = 0;

// For vibration output
#define LEFT_VIBRATION_PIN 13
#define RIGHT_VIBRATION_PIN 11
#define LEFT_VIBRATION_PWM_CHANNEL 2
#define RIGHT_VIBRATION_PWM_CHANNEL 3
#define LEFT_VIBRATION_PWM_RESOLUTION 8
#define RIGHT_VIBRATION_PWM_RESOLUTION 8

// setup two I2C busses.
#define LPn_PIN_1 2
#define SDA_PIN_1 4
#define SCL_PIN_1 3
ToFSensor sensor1(&Wire, LPn_PIN_1, sensorSize, 1);

#define LPn_PIN_2 7
#define SDA_PIN_2 6
#define SCL_PIN_2 5
ToFSensor sensor2(&Wire1, LPn_PIN_2, sensorSize, 2);

bool detectLeft = false;
bool detectRight = false;
bool detectMid = false;

// data buffer
uint8_t bufferIndex = 0;
uint8_t bufferLength = 12;
// list of lists. (dynamic size) init directly based on size, and combinedframe resolution
std::vector<std::vector<uint16_t>> rawDataBuffer(
    bufferLength, std::vector<uint16_t>(sensorSize * numberOfSensors)
);

uint8_t storedLeftIntensity = 0;
uint8_t storedMidIntensity = 0;
uint8_t storedRightIntensity = 0;

void giveUserFeedback(uint8_t leftIntensity, uint8_t middleIntensity, uint8_t rightIntensity);

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

    // init TOF sensors
    // I2C: sda=5, scl=6
    Wire.begin(SDA_PIN_1, SCL_PIN_1);
    Wire.setClock(800000);  // 800kHz

    // I2C: sda=3, scl=2
    Wire1.begin(SDA_PIN_2, SCL_PIN_2);
    Wire1.setClock(800000);  // 800kHz

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


    // timer setup for speakers & vibration
    feedback_timer = timerBegin(1, 80, true); // use timer 1 to avoid conflict with echo sensor timer 0
    timerAttachInterrupt(feedback_timer, &onFeedbackTimerISR, true);
    timerAlarmWrite(feedback_timer, 100000, false); // 100 ms
    timerAlarmDisable(feedback_timer);


    // speaker setup
    pinMode(LEFT_SPEAKER_PIN, OUTPUT);
    ledcSetup(LEFT_SPEAKER_PWM_CHANNEL, 1000, LEFT_SPEAKER_PWM_RESOLUTION);
    ledcAttachPin(LEFT_SPEAKER_PIN, LEFT_SPEAKER_PWM_CHANNEL);
    ledcWrite(LEFT_SPEAKER_PWM_CHANNEL, 0);
    pinMode(RIGHT_SPEAKER_PIN, OUTPUT);
    ledcSetup(RIGHT_SPEAKER_PWM_CHANNEL, 1000, RIGHT_SPEAKER_PWM_RESOLUTION);
    ledcAttachPin(RIGHT_SPEAKER_PIN, RIGHT_SPEAKER_PWM_CHANNEL);
    ledcWrite(RIGHT_SPEAKER_PWM_CHANNEL, 0);


    // vibration setup
    pinMode(LEFT_VIBRATION_PIN, OUTPUT);
    ledcSetup(LEFT_VIBRATION_PWM_CHANNEL, 1000, LEFT_VIBRATION_PWM_RESOLUTION);
    ledcAttachPin(LEFT_VIBRATION_PIN, LEFT_VIBRATION_PWM_CHANNEL);
    ledcWrite(LEFT_VIBRATION_PWM_CHANNEL, 0);
    pinMode(RIGHT_VIBRATION_PIN, OUTPUT);
    ledcSetup(RIGHT_VIBRATION_PWM_CHANNEL, 1000, RIGHT_VIBRATION_PWM_RESOLUTION);
    ledcAttachPin(RIGHT_VIBRATION_PIN, RIGHT_VIBRATION_PWM_CHANNEL);
    ledcWrite(RIGHT_VIBRATION_PWM_CHANNEL, 0);


    //start up test for outputs.
    giveUserFeedback(50, 0, 0);
    Serial.println("Left feedback 50");
    delay(500);
    giveUserFeedback(0, 0, 50);
    Serial.println("Right feedback 50");
    delay(500);
    giveUserFeedback(0, 50, 0);
    Serial.println("Middle feedback 50");
    delay(500);



//     echosensor.begin();
//     echosensor.trigger();
//     while (1) {
//         if(echosensor.newDataAvailable()) {
//             if (echosensor.timedOut()) {
//                 Serial.println("Echo Sensor: No object detected within range.");
//             } else {
//                 echoDistance = echosensor.getDistanceCM();
//                 Serial.print("Echo Sensor Distance: ");
//                 Serial.print(echoDistance);
//                 Serial.println(" cm");
//             }
//             echosensor.trigger();
//         }
//     }


    pixels.setPixelColor(0, pixels.Color(0, 255, 0));  // Green
    pixels.show();
    previous_call = millis();
}

uint8_t distanceToIntensity(uint16_t distance) {
    if (distance == 0) return 0;

    if (distance <= 200) return 100;
    else if (distance <= 400) return 80;
    else if (distance <= 600) return 60;
    else if (distance <= 800) return 40;
    else if (distance <= 1000) return 20;
    else return 0;
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

            Serial.print(zone.label);
            Serial.print(": intensity = ");
            Serial.println(intensity);
        }
    }
}

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

// Give user feedback based on the detected distance in each sector (left, middle, right). Values can range from 0 (no feedback) to 100 (most intense feedback).
void giveUserFeedback(uint8_t leftIntensity, uint8_t middleIntensity, uint8_t rightIntensity) {
    leftIntensity = max(leftIntensity, middleIntensity);
    rightIntensity = max(rightIntensity, middleIntensity);

    if (leftIntensity == 0 && rightIntensity == 0) {
        stopFeedback();
        return;
    }

    bool hasActivity = false;


    // SPEAKER
    // Map intensity to an audible frequency range.
    // Higher intensity produces a higher pitched tone.
    uint32_t leftFreq = 0;
    uint32_t rightFreq = 0;
    if (leftIntensity) leftFreq = map(leftIntensity, 0, 100, MIN_FREQ, MAX_FREQ);
    if (rightIntensity) rightFreq = map(rightIntensity, 0, 100, MIN_FREQ, MAX_FREQ);
    if (leftFreq || rightFreq) {
        if (leftFreq) {
            ledcWriteTone(LEFT_SPEAKER_PWM_CHANNEL, leftFreq);
            ledcWrite(LEFT_SPEAKER_PWM_CHANNEL, LEFT_SPEAKER_DEFAULT_DUTY);
        } else ledcWrite(LEFT_SPEAKER_PWM_CHANNEL, 0);

        if (rightFreq) {
            ledcWriteTone(RIGHT_SPEAKER_PWM_CHANNEL, rightFreq);
            ledcWrite(RIGHT_SPEAKER_PWM_CHANNEL, RIGHT_SPEAKER_DEFAULT_DUTY);
        } else ledcWrite(RIGHT_SPEAKER_PWM_CHANNEL, 0);

        hasActivity = true;
    }


    // VIBRATION
    uint8_t leftvibration = map(leftIntensity, 0, 100, 50, 255);        // below 50 the vibration motor will not work
    uint8_t rightvibration = map(rightIntensity, 0, 100, 50, 255);
    if (leftIntensity || rightIntensity) {
        if (leftIntensity) ledcWrite(LEFT_VIBRATION_PWM_CHANNEL, leftvibration);
        if (rightIntensity) ledcWrite(RIGHT_VIBRATION_PWM_CHANNEL, rightvibration);
        hasActivity = true;
    }


    // Start timer only once, after all pins are written
    if (hasActivity) {
        startFeedbackTimer();
    }
}

void IRAM_ATTR onFeedbackTimerISR() {
    // ISR: avoid calling non-ISR-safe functions (like ledcWrite) here.
    // Set a flag and disable the alarm; main loop will handle stopping outputs.
    feedbackTimerFired = true;
    timerAlarmDisable(feedback_timer);
}

void startFeedbackTimer() {
    timerAlarmDisable(feedback_timer);  // Disable first to clear any pending interrupt
    timerWrite(feedback_timer, 0);      // Reset timer counter to 0
    timerAlarmWrite(feedback_timer, feedback_duration, false);              // 1000000 us = 1000 ms
    timerAlarmEnable(feedback_timer);
}

void stopFeedback() {
    #ifdef SPEAKER_OUTPUT
    ledcWrite(LEFT_SPEAKER_PWM_CHANNEL, 0);
    ledcWrite(RIGHT_SPEAKER_PWM_CHANNEL, 0);
    #endif
    #ifdef VIBRATION_OUTPUT
    ledcWrite(LEFT_VIBRATION_PWM_CHANNEL, 0);
    ledcWrite(RIGHT_VIBRATION_PWM_CHANNEL, 0);
    #endif
    timerAlarmDisable(feedback_timer);
    timerWrite(feedback_timer, 0);      // Reset timer counter
}

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

void loop() {
    // if ISR signalled, stop outputs here (safe context)
    if (feedbackTimerFired) {
        #ifdef SPEAKER_OUTPUT
        ledcWrite(LEFT_SPEAKER_PWM_CHANNEL, 0);
        ledcWrite(RIGHT_SPEAKER_PWM_CHANNEL, 0);
        #endif
        #ifdef VIBRATION_OUTPUT
        ledcWrite(LEFT_VIBRATION_PWM_CHANNEL, 0);
        ledcWrite(RIGHT_VIBRATION_PWM_CHANNEL, 0);
        #endif
        feedbackTimerFired = false;
        Serial.println("USER FEEDBACK timer: stopped outputs (handled in loop)");
    }
    bool sensor1Ready = sensor1.getSensorReady();
    bool sensor2Ready = sensor2.getSensorReady();

    // if both sensors are ready, process the data.
    if (sensor1Ready && sensor2Ready) {
    //if (sensor2Ready) {
        
        const std::vector<uint16_t> &data1_ref = sensor1.fetchRawData();
        //const std::vector<uint16_t> &data1_ref = sensor2.fetchRawData();
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

        
        // detect close objects
        // write to pointer of local

        detectCloseObject(averagedGrid, storedLeftIntensity, storedMidIntensity, storedRightIntensity, 1000, 0.40);
    }
      
    // read echo sensor data (non-blocking)
    // if(echosensor.newDataAvailable()) {
    //     if (echosensor.timedOut()) {
    //         Serial.println("Echo Sensor: No object detected within range.");
    //     } else {
    //         echoDistance = echosensor.getDistanceCM();
    //         echocount++;
    //         echototal = echototal + echoDistance;
    //     }
    //     echosensor.trigger();
    // }

    // give user feedback
    if (millis() - previous_call >= feedback_interval || previous_call > millis()) {     // also account for millis() overflow
        previous_call = millis();
        
        // convert echo reading
        // uint16_t average_echo = (echocount > 0) ? (echototal / echocount) : 0;
        // uint8_t echo_intensity = distanceToIntensity(average_echo * 10);

        uint8_t echo_intensity = 0; 

        // update the intensity settings to give a new 100ms feedback pulse, every second.
        giveUserFeedback(storedLeftIntensity, max(storedMidIntensity, echo_intensity), storedRightIntensity);
        echototal = 0;
        echocount = 0;
    }
}

