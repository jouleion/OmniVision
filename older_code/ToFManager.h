// #pragma once

// #include <Arduino.h>
// #include <vector>


// #define NUM_TOF_SENSORS 4
// int TOF_LPN_PINS[] = { 2, 3, 4, 5 }; // Example LPN pins for each sensor


// // This is the VL53L7CX ToF Sensor
// // https://www.st.com/resource/en/datasheet/vl53l7cx.pdf
// //  matrix of 8x8 zones (64 zones) and can work at fast speeds (60 Hz) up to 350 cm.
// // C: serial bus, address: 0x52

// // Simplified sensor class for TOF sensor
// class ToFManager {
// public:
//     // Construct manager. You may pass arrays for pins; if nullptr, defaults from SystemConfig are used.
//     ToFManager(TwoWire &i2c = Wire,
//                int numSensors = NUM_TOF_SENSORS,
//                const int *lpnPins = TOF_LPN_PINS,
//                const int *i2cRstPins = nullptr,
//                const int *pwrenPins = nullptr);

//     ~ToFManager();

//     // Initialize all sensors. Returns true if at least one sensor started successfully.
//     bool begin();

//     // Read the first detected distance for sensor `id` (1-based).
//     // Returns distance in mm, or -1 on error/no-target/invalid id.
//     int readFirstOfSensor(int id);

//     // Stop all sensors and release resources.
//     void stopAll();

// private:
//     TwoWire * _i2c;
//     int       _numSensors;
//     std::vector<int> _lpnPins;
//     std::vector<int> _i2cRstPins;
//     std::vector<int> _pwrenPins;
//     std::vector<std::unique_ptr<VL53L7CX>> _sensors;
//     uint8_t _resolution;
// };
