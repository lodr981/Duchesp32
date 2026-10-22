#pragma once
// QMI8658 accelerometer on the Waveshare ESP32-S3-LCD-1.28 (I2C SDA 6, SCL 7)
// with gesture detection running in its own task at 100 Hz.
#include <Arduino.h>

enum ImuEvent : uint8_t {
  IMU_NONE,
  IMU_SHAKE,         // a few quick shakes
  IMU_STRONG_SHAKE,  // shaken for a longer time
  IMU_DOUBLE_TAP,
  IMU_MOTION,        // moved after being still (wake up)
};

struct ImuState {
  bool ok;
  float lean;        // -1..1 relative to the calibrated hanging position
  bool upsideDown;
  bool lyingFlat;    // display facing up or down
  uint32_t stillMs;  // how long the pendant has not moved
  float gx, gy, gz;  // filtered gravity (g)
};

bool imuInit(float refX, float refY, float refZ, bool invert, uint8_t sensitivity);
void imuConfigure(bool invert, uint8_t sensitivity);
ImuState imuState();
ImuEvent imuPollEvent();  // returns IMU_NONE when the queue is empty
// Uses the current gravity as the "hanging straight" reference.
bool imuCalibrate(float &refX, float &refY, float &refZ);
// True once after the reference was set automatically (first calm moment, not calibrated yet).
bool imuReferenceChanged(float &refX, float &refY, float &refZ);
