// class header for ToF sensor object.
#pragma once
#include <vl53l7cx_class.h>
#include <vector>

enum SensorSize{
    SIZE_4X4 = 16,
    SIZE_8X8 = 64
};

class ToFSensor {
    public:
        ToFSensor(TwoWire *wire, uint8_t lpn_pin, SensorSize size, uint8_t sensorId = 0);

        // with sensor parameters.
        bool begin(SensorSize newSize, uint8_t frequency, uint8_t integrationTime, uint8_t sharpenerPercent);
        bool getSensorReady();
        const std::vector<uint16_t>& fetchRawData() const;

        void setSize(SensorSize newSize);
        void setFrequency(uint8_t frequency);

    private:
        TwoWire *wire;
        VL53L7CX sensor;
        uint8_t sensorId;
        SensorSize size;
        std::vector<uint16_t> rawDataGrid; // x by x grid
        bool ready = false;
        uint8_t lpn_pin;
        
};
