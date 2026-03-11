#include <Wire.h>
#include <VL53L1X.h>

VL53L1X sensor;
#define LPN_PIN 2  // Your LPN pin → Arduino pin 2

void setup() {
    Serial.begin(9600);
    
    // CRITICAL: LPN reset sequence
    pinMode(LPN_PIN, OUTPUT);
    digitalWrite(LPN_PIN, LOW);   // Shutdown
    delay(500);
    digitalWrite(LPN_PIN, HIGH);  // Enable
    delay(1000);  // 1 SECOND power-up!!!
    
    Wire.begin();
    Wire.setClock(50000);  // 50kHz - SUPER SLOW for Uno
    
    Serial.println("VL53L1X Pololu - LPN reset");
    
    if (!sensor.init()) {
        Serial.println("init FAILED - CHECK 3.3V + LPN wiring");
        while(1);
    }
    
    Serial.println("Sensor OK!");
    sensor.setTimeout(1000);
    sensor.startContinuous(100);
}

void loop() {
    int distance = sensor.read();
    if (sensor.timeoutOccurred()) {
        Serial.println("TIMEOUT");
    } else {
        Serial.print(distance);
        Serial.println(" mm");
    }
    delay(200);
}
