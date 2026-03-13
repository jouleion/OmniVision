#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <vl53l7cx_class.h>  // Latest STM32duino header

#define NEOPIXEL_PIN 21
#define NUM_PIXELS 1
Adafruit_NeoPixel pixels(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// use the core-provided I2C objects `Wire` and `Wire1`

#define LPn_PIN_1 4
#define SDA_PIN_1 5
#define SCL_PIN_1 6

VL53L7CX sensor1(&Wire, LPn_PIN_1);

#define LPn_PIN_2 7
#define SDA_PIN_2 3
#define SCL_PIN_2 2
VL53L7CX sensor2(&Wire1, LPn_PIN_2);

unsigned long lastBlink = 0;
void nonBlockingBlinkError();
void readToFSensor(VL53L7CX &sensor, uint8_t sensorId);

void setup() {
    Serial.begin(115200);
    delay(3000);
    Serial.println("ESP32-S3 + VL53L7CX v1.0.3 + NeoPixel");
    while(!Serial);

    // NeoPixel
    pixels.begin();
    pixels.clear();
    pixels.show();

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
    Wire.setClock(800000);  // 100kHz

    Wire1.begin(SDA_PIN_2, SCL_PIN_2);
    Wire1.setClock(800000);  // 100kHz


    // VL53L7CX init sensor 1
    Serial.print("VL53L7CX 1 init...");
    sensor1.begin();
    int init_status = sensor1.init_sensor();

    if (init_status != VL53L7CX_STATUS_OK) {
        Serial.print("FAILED 1 (status: "); Serial.print(init_status); Serial.println(")");
        pixels.setPixelColor(0, pixels.Color(255, 0, 0));  // Red ERROR
        pixels.show();
        while(1) { nonBlockingBlinkError(); }
    }

    pixels.setPixelColor(0, pixels.Color(255, 165, 0));  // Orange
    pixels.show();
    delay(100);

    // 1. Set 8x8 RESOLUTION (not default 4x4!)
    sensor1.vl53l7cx_set_resolution(VL53L7CX_RESOLUTION_8X8);

    // 2. Set 60Hz frequency (max speed)
    sensor1.vl53l7cx_set_ranging_frequency_hz(60);

    // 3. Short integration time for speed
    sensor1.vl53l7cx_set_integration_time_ms(5);  // 5ms = fast

    // 4. Increase sensitivity
    sensor1.vl53l7cx_set_sharpener_percent(80);  // More zones trigger

    pixels.setPixelColor(0, pixels.Color(255, 255, 0));  // Yellow for config done
    pixels.show();

    // Start ranging (continuous mode, 8x8 zones)
    int start_status = sensor1.vl53l7cx_start_ranging();
    if (start_status != VL53L7CX_STATUS_OK) {
        Serial.println("Start ranging 1 FAILED");
    } else {
        Serial.println("VL53L7CX 1 READY - 8x8 zones active");
        pixels.setPixelColor(0, pixels.Color(0, 255, 0));  // Green OK
        pixels.show();
        delay(100);
    }

    pixels.setPixelColor(0, pixels.Color(100, 0, 255));  // Blue for next sensor
    pixels.show();
    delay(100);

    // sensor 2
    // VL53L7CX init sensor 2
    Serial.print("VL53L7CX 2 init...");
    sensor2.begin();
    init_status = sensor2.init_sensor();

    if (init_status != VL53L7CX_STATUS_OK) {
        Serial.print("FAILED 2 (status: "); Serial.print(init_status); Serial.println(")");
        pixels.setPixelColor(0, pixels.Color(255, 0, 0));  // Red ERROR
        pixels.show();
        while(1) { nonBlockingBlinkError(); }
    }

    pixels.setPixelColor(0, pixels.Color(255, 165, 0));  // Orange
    pixels.show();
    delay(100);

    // 1. Set 8x8 RESOLUTION (not default 4x4!)
    sensor2.vl53l7cx_set_resolution(VL53L7CX_RESOLUTION_8X8);

    // 2. Set 60Hz frequency (max speed)
    sensor2.vl53l7cx_set_ranging_frequency_hz(60);

    // 3. Short integration time for speed
    sensor2.vl53l7cx_set_integration_time_ms(5);  // 5ms = fast

    // 4. Increase sensitivity
    sensor2.vl53l7cx_set_sharpener_percent(80);  // More zones trigger

    pixels.setPixelColor(0, pixels.Color(255, 255, 0));  // Yellow for config done
    pixels.show();

    // Start ranging (continuous mode, 8x8 zones)
    start_status = sensor2.vl53l7cx_start_ranging();
    if (start_status != VL53L7CX_STATUS_OK) {
        Serial.println("Start ranging 2 FAILED");
    } else {
        Serial.println("VL53L7CX 2 READY - 8x8 zones active");
        pixels.setPixelColor(0, pixels.Color(0, 255, 0));  // Green OK
        pixels.show();
    }
}


void loop() {
    readToFSensor(sensor1, 1);
    readToFSensor(sensor2, 2);
}


void readToFSensor(VL53L7CX &sensor, uint8_t sensorId) {
    uint8_t new_data_ready = 0;
    sensor.vl53l7cx_check_data_ready(&new_data_ready);

    if (new_data_ready) {
        VL53L7CX_ResultsData results;
        int status = sensor.vl53l7cx_get_ranging_data(&results);
        
        if (status == VL53L7CX_STATUS_OK) {
            Serial.print("Sensor "); 
            Serial.print(sensorId); 
            Serial.print(": ");
            Serial.println("\n8x8 GRID");
            // Print 8x8 distance grid (64 zones total)
            for (int row = 0; row < 8; row++) {
                for (int col = 0; col < 8; col++) {
                    int zone = row * 8 + col;
                    uint16_t distance = results.distance_mm[zone];
                    
                    // Print distance (pad to 4 chars)
                    if (distance < 1000) Serial.print(" ");
                    if (distance < 100) Serial.print(" ");
                    if (distance < 10) Serial.print(" ");
                    
                    Serial.print(distance);
                    Serial.print(" ");
                }
                Serial.println();  // New line after row
            }
        }
    }
}

void nonBlockingBlinkError() {
    if (millis() - lastBlink > 300) {
        static bool ledState = false;
        if (ledState) {
        pixels.setPixelColor(0, pixels.Color(255, 0, 0));
        } else {
        pixels.clear();
        }
        pixels.show();
        ledState = !ledState;
        lastBlink = millis();
    }
}
