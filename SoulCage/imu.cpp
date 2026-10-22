#include "imu.h"
#include <Wire.h>
#include <freertos/queue.h>
#include <math.h>

#define IMU_SDA 6
#define IMU_SCL 7
#define REG_WHO_AM_I 0x00
#define REG_CTRL1 0x02
#define REG_CTRL2 0x03
#define REG_CTRL7 0x08
#define REG_AX_L 0x35
#define REG_RESET 0x60
#define LSB_PER_G 8192.0f  // +-4 g range

static uint8_t address;
static QueueHandle_t events;
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static ImuState shared;
static float refX, refY, refZ;
static volatile bool invertLean;
static volatile float thresholdScale = 1.0f;
static volatile bool refChanged;

static bool writeReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

static bool readRegs(uint8_t reg, uint8_t *buf, size_t len) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  if (Wire.requestFrom((uint16_t)address, len) != len) {
    return false;
  }
  for (size_t i = 0; i < len; i++) {
    buf[i] = Wire.read();
  }
  return true;
}

static void emit(ImuEvent e) {
  uint8_t v = e;
  xQueueSend(events, &v, 0);
}

static void imuTask(void *) {
  float gx = 0, gy = 0, gz = 0;
  bool primed = false;
  uint32_t peaks[16] = {0};
  uint8_t peakHead = 0;
  uint32_t lastPeak = 0, lastShake = 0, lastStrong = 0;
  uint32_t quietSince = 0, stillSince = millis();
  bool wasStill = false;
  // Tap detection
  uint32_t tapStart = 0, lastTap = 0;
  uint8_t tapHigh = 0;
  float prevMag = 0;

  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(10));
    uint8_t raw[6];
    if (!readRegs(REG_AX_L, raw, 6)) {
      continue;
    }
    float ax = (int16_t)(raw[0] | raw[1] << 8) / LSB_PER_G;
    float ay = (int16_t)(raw[2] | raw[3] << 8) / LSB_PER_G;
    float az = (int16_t)(raw[4] | raw[5] << 8) / LSB_PER_G;
    if (!primed) {
      gx = ax, gy = ay, gz = az;
      primed = true;
    }
    gx += (ax - gx) * 0.08f;
    gy += (ay - gy) * 0.08f;
    gz += (az - gz) * 0.08f;
    float dx = ax - gx, dy = ay - gy, dz = az - gz;
    float mag = sqrtf(dx * dx + dy * dy + dz * dz);
    uint32_t now = millis();
    float k = thresholdScale;

    // Shakes: count acceleration peaks.
    if (mag > 0.9f * k && now - lastPeak > 80) {
      lastPeak = now;
      peaks[peakHead++ & 15] = now;
      int recent = 0, longer = 0;
      for (uint32_t p : peaks) {
        if (p && now - p < 1000) recent++;
        if (p && now - p < 2500) longer++;
      }
      if (longer >= 12 && now - lastStrong > 3000) {
        lastStrong = lastShake = now;
        emit(IMU_STRONG_SHAKE);
      } else if (recent >= 4 && now - lastShake > 1500) {
        lastShake = now;
        emit(IMU_SHAKE);
      }
    }

    // Taps: a short spike after a quiet moment.
    if (mag < 0.25f * k) {
      if (!quietSince) quietSince = now;
    }
    if (!tapStart && mag > 1.0f * k && prevMag <= 1.0f * k && quietSince && now - quietSince > 100) {
      tapStart = now;
      tapHigh = 0;
    }
    if (tapStart) {
      if (mag > 0.5f * k) tapHigh++;
      if (now - tapStart > 120) {
        if (tapHigh <= 5) {
          if (lastTap && tapStart - lastTap > 100 && tapStart - lastTap < 600) {
            emit(IMU_DOUBLE_TAP);
            lastTap = 0;
          } else {
            lastTap = tapStart;
          }
        }
        tapStart = 0;
      }
    }
    if (mag >= 0.25f * k) quietSince = 0;
    prevMag = mag;

    // Stillness and waking up.
    if (mag > 0.12f * k) {
      if (wasStill && now - stillSince > 5000) emit(IMU_MOTION);
      stillSince = now;
      wasStill = false;
    } else if (mag < 0.05f) {
      wasStill = true;
    }

    // Orientation relative to the calibrated hanging position (screen plane = x/y).
    float len = sqrtf(gx * gx + gy * gy + gz * gz);
    float plane = sqrtf(gx * gx + gy * gy);
    float lean = 0, angle = 0;
    portENTER_CRITICAL(&mux);
    float rx = refX, ry = refY;
    portEXIT_CRITICAL(&mux);
    float rl = sqrtf(rx * rx + ry * ry);
    if (rl < 0.3f && now - stillSince > 2000 && plane > 0.7f) {
      // Not calibrated yet: the first calm moment becomes "hanging straight".
      portENTER_CRITICAL(&mux);
      refX = gx, refY = gy, refZ = gz;
      portEXIT_CRITICAL(&mux);
      refChanged = true;
    }
    if (rl >= 0.3f && plane > 0.3f) {
      angle = atan2f(rx * gy - ry * gx, rx * gx + ry * gy) * 57.29578f;
      lean = angle / 40.0f;
      lean = lean > 1 ? 1 : lean < -1 ? -1 : lean;
      if (invertLean) lean = -lean;
    }
    portENTER_CRITICAL(&mux);
    shared.ok = true;
    shared.lean = lean;
    shared.upsideDown = rl >= 0.3f && plane > 0.5f && fabsf(angle) > 135;
    shared.lyingFlat = len > 0.5f && fabsf(gz) > 0.85f * len;
    shared.stillMs = now - stillSince;
    shared.gx = gx, shared.gy = gy, shared.gz = gz;
    portEXIT_CRITICAL(&mux);
  }
}

bool imuInit(float rx, float ry, float rz, bool invert, uint8_t sensitivity) {
  refX = rx, refY = ry, refZ = rz;
  imuConfigure(invert, sensitivity);
  Wire.begin(IMU_SDA, IMU_SCL, 400000);
  const uint8_t candidates[] = {0x6B, 0x6A};
  for (uint8_t a : candidates) {
    address = a;
    uint8_t who = 0;
    if (readRegs(REG_WHO_AM_I, &who, 1) && who == 0x05) {
      writeReg(REG_RESET, 0xB0);
      delay(20);
      bool ok = writeReg(REG_CTRL1, 0x40)     // register address auto increment
                && writeReg(REG_CTRL2, 0x16)  // accelerometer +-4 g, ~112 Hz
                && writeReg(REG_CTRL7, 0x01);  // accelerometer on, gyro off
      if (!ok) {
        break;
      }
      events = xQueueCreate(8, sizeof(uint8_t));
      xTaskCreatePinnedToCore(imuTask, "imu", 4096, nullptr, 2, nullptr, 0);
      Serial.printf("IMU: QMI8658 at 0x%02X\n", a);
      return true;
    }
  }
  Serial.println("IMU: QMI8658 not found");
  return false;
}

void imuConfigure(bool invert, uint8_t sensitivity) {
  invertLean = invert;
  static const float scales[] = {1.5f, 1.0f, 0.7f};  // low, normal, high sensitivity
  thresholdScale = scales[sensitivity > 2 ? 1 : sensitivity];
}

ImuState imuState() {
  portENTER_CRITICAL(&mux);
  ImuState s = shared;
  portEXIT_CRITICAL(&mux);
  return s;
}

ImuEvent imuPollEvent() {
  uint8_t v;
  if (events && xQueueReceive(events, &v, 0) == pdTRUE) {
    return (ImuEvent)v;
  }
  return IMU_NONE;
}

bool imuCalibrate(float &rx, float &ry, float &rz) {
  ImuState s = imuState();
  if (!s.ok) {
    return false;
  }
  portENTER_CRITICAL(&mux);
  refX = s.gx, refY = s.gy, refZ = s.gz;
  portEXIT_CRITICAL(&mux);
  rx = s.gx, ry = s.gy, rz = s.gz;
  return true;
}

bool imuReferenceChanged(float &rx, float &ry, float &rz) {
  if (!refChanged) {
    return false;
  }
  refChanged = false;
  portENTER_CRITICAL(&mux);
  rx = refX, ry = refY, rz = refZ;
  portEXIT_CRITICAL(&mux);
  return true;
}
