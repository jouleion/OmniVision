// #include "ToFManager.h"

// ToFManager::ToFManager(TwoWire &i2c, int numSensors, const int *lpnPins, const int *i2cRstPins, const int *pwrenPins)
//     : _i2c(&i2c), _numSensors(numSensors), _resolution(VL53L7CX_RESOLUTION_8X8)
// {
//     _lpnPins.resize(_numSensors, -1);
//     _i2cRstPins.resize(_numSensors, -1);
//     _pwrenPins.resize(_numSensors, -1);
//     _sensors.resize(_numSensors);

//     if (lpnPins) {
//         for (int i = 0; i < _numSensors; ++i) _lpnPins[i] = lpnPins[i];
//     }
//     if (i2cRstPins) {
//         for (int i = 0; i < _numSensors; ++i) _i2cRstPins[i] = i2cRstPins[i];
//     }
//     if (pwrenPins) {
//         for (int i = 0; i < _numSensors; ++i) _pwrenPins[i] = pwrenPins[i];
//     }
// }

// ToFManager::~ToFManager()
// {
//     stopAll();
// }

// bool ToFManager::begin()
// {
//     if (!_i2c) return false;

//     _i2c->begin();

//     bool any_ok = false;
//     for (int i = 0; i < _numSensors; ++i) {
//         // power enable if provided
//         if (_pwrenPins[i] >= 0) {
//             pinMode(_pwrenPins[i], OUTPUT);
//             digitalWrite(_pwrenPins[i], HIGH);
//             delay(10);
//         }

//         // create sensor object
//         _sensors[i] = std::make_unique<VL53L7CX>(_i2c, _lpnPins[i], _i2cRstPins[i]);
//         if (!_sensors[i]) continue;

//         if (_sensors[i]->begin() != 0) {
//             _sensors[i].reset();
//             continue;
//         }

//         if (_sensors[i]->init_sensor() != 0) {
//             _sensors[i].reset();
//             continue;
//         }

//         _sensors[i]->vl53l7cx_set_resolution(_resolution);
//         _sensors[i]->vl53l7cx_start_ranging();
//         any_ok = true;
//     }

//     return any_ok;
// }

// int ToFManager::readFirstOfSensor(int id)
// {
//     if (id < 1 || id > _numSensors) return -1;
//     int idx = id - 1;
//     if (!_sensors[idx]) return -1;

//     VL53L7CX_ResultsData results;
//     uint8_t newData = 0;

//     unsigned long start = millis();
//     while (true) {
//         if (_sensors[idx]->vl53l7cx_check_data_ready(&newData) != 0)
//             return -1;
//         if (newData) break;
//         if (millis() - start > 200) return -1;
//         delay(5);
//     }

//     if (_sensors[idx]->vl53l7cx_get_ranging_data(&results) != 0)
//         return -1;

//     if (results.nb_target_detected[0] > 0) return (int)results.distance_mm[0];
//     return -1;
// }

// void ToFManager::stopAll()
// {
//     for (int i = 0; i < _numSensors; ++i) {
//         if (_sensors[i]) {
//             _sensors[i]->vl53l7cx_stop_ranging();
//             _sensors[i].reset();
//         }
//     }
// }

