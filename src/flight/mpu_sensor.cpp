// mpu_sensor.cpp — siehe Header. electroniccats/MPU6050, HW-I2C, Adresse 0x68.
#include "mpu_sensor.h"
#include <Arduino.h>
#include <MPU6050.h>

namespace {
MPU6050 g_mpu;          // Default-Adresse 0x68 (AD0=LOW)
bool    g_ok = false;

// Skalierung Rohwert -> physikalische Einheit bei den Lib-Default-Ranges:
//   Accel ±2 g    -> 16384 LSB/g
//   Gyro  ±250°/s -> 131 LSB/(°/s)
constexpr float kAccelLsbPerG   = 16384.0f;
constexpr float kGyroLsbPerDps  = 131.0f;
}  // namespace

bool mpu_begin() {
    g_mpu.initialize();
    g_ok = g_mpu.testConnection();
    return g_ok;
}

void mpu_read(telemetry::TelemetryRecord& rec) {
    if (!g_ok) return;

    int16_t ax, ay, az, gx, gy, gz;
    g_mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

    rec.has_mpu   = true;
    rec.acc_x_g   = ax / kAccelLsbPerG;
    rec.acc_y_g   = ay / kAccelLsbPerG;
    rec.acc_z_g   = az / kAccelLsbPerG;
    rec.gyr_x_dps = gx / kGyroLsbPerDps;
    rec.gyr_y_dps = gy / kGyroLsbPerDps;
    rec.gyr_z_dps = gz / kGyroLsbPerDps;
}
