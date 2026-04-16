# OmniVision Project

## Overview
The OmniVision project is designed to utilize an ESP32-S3 Mini microcontroller to integrate various hardware components and provide a robust system for object detection and feedback. This document provides an overview of the hardware setup and the C++ implementation.

---

## Hardware Components

### Microcontroller
- **ESP32-S3 Mini**: A powerful microcontroller with an internal battery charger. Ensure the battery is turned on and connected via USB-C for charging.

### Sensors
- **Time-of-Flight (ToF) Sensors**: Two VL53L7CX ToF sensors are used for precise distance measurement. These sensors operate in either 4x4 or 8x8 resolution modes.
- **Echo Sensor**: HC-SR04 ultrasonic sensor for additional distance measurement.

### Feedback Devices
- **Piezo Buzzers**: Two buzzers for audio feedback.
- **Vibration Motors**: Two motors with transistor-based control and flyback diodes for haptic feedback.

### LED
- **NeoPixel LED**: A single RGB LED for visual feedback.

---

## Pin Configuration

### ToF Sensors
- **Sensor 1**: 
  - LPn: Pin 2
  - SDA: Pin 4
  - SCL: Pin 3
- **Sensor 2**: 
  - LPn: Pin 7
  - SDA: Pin 6
  - SCL: Pin 5

### Echo Sensor
- **Trigger Pin**: Pin 8
- **Echo Pin**: Pin 1

### Feedback Devices
- **Left Speaker**: Pin 10
- **Right Speaker**: Pin 9
- **Left Vibration Motor**: Pin 13
- **Right Vibration Motor**: Pin 15

### NeoPixel LED
- **Data Pin**: Pin 48

---

## C++ Implementation

### Key Features
- **ToF Sensor Integration**: The `ToFSensor` class handles initialization, configuration, and data retrieval from the VL53L7CX sensors. It supports dynamic resolution and frequency adjustments.
- **Echo Sensor Integration**: The `EchoSensor` class provides non-blocking distance measurements.
- **Feedback Mechanism**: Audio and haptic feedback are controlled using PWM signals. The `writeFeedback` function maps intensity values to duty cycles and frequencies.
- **Data Processing**: Includes functions for combining sensor grids, averaging frames, and detecting objects in specific zones (left, middle, right).
- **LED Control**: The NeoPixel LED is used for status indication during initialization and error handling.

### Code Structure
- **`main.cpp`**: Contains the main application logic, including setup and loop functions.
- **`tof.cpp` and `tof.h`**: Implements the `ToFSensor` class for ToF sensor management.
- **`echo_sensor.cpp` and `echo_sensor.h`**: Implements the `EchoSensor` class for ultrasonic sensor management.
- **`system_parameters.h`**: Defines system-wide constants and parameters.

### Setup Functions
- **`setupFeedback`**: Initializes PWM channels for speakers and vibration motors.
- **`setupLed`**: Configures the NeoPixel LED.
- **`setupTOF`**: Initializes the ToF sensors with appropriate I2C settings and resolution.

### Loop Function
The `loop` function handles:
- Reading data from tof sensors.
- Combine 2 frames into 1 frame
- create data buffer
- average buffer (cell wise average)
- detect distance threshold
- generate pwm signals (speaker & vibration motors)


## Uploading software
Please use platformio extension in VSCode to upload via serial. Please do not install platform arduino extension. platformio should work as is. Restart VSCode to troubleshoot.