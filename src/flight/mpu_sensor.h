// mpu_sensor.h — MPU-6050 (IMU: Beschleunigung + Drehrate) via electroniccats-Lib.
//
// Board-Hardware-Teil (kein nativer Test, analog zu bmp_sensor.cpp) — wird am
// Heltec V2 verifiziert. I²C-Adresse fest 0x68 (AD0=LOW, Modul-Default) am
// selben Bus wie BMP280/OLED (Wire.begin(4,15) in main.cpp).
//
// WOZU? Die IMU zeigt Lage, Rotation und Taumeln des Ballons im Flug. Geloggt
// werden Beschleunigung (g) und Drehrate (°/s) — physikalische Einheiten wie
// bei den anderen Sensoren, damit die Werte direkt lesbar sind.
//
// Ranges: ±2 g / ±250 °/s (Lib-Default). Reicht fürs Taumeln/Lage; höhere
// Bereiche erst, wenn ein Messwert tatsächlich ansteht (YAGNI).
//
// KEIN async nötig (anders als DS18B20): getMotion6() ist ein einzelner
// I²C-Burst und dauert nur µs.
#ifndef FLIGHT_MPU_SENSOR_H
#define FLIGHT_MPU_SENSOR_H

#include "record.h"

// I²C muss vorher laufen (Wire.begin(4,15) in main.cpp/setup()).
// Rückgabe: true, wenn der Sensor am Bus antwortet (testConnection()).
bool mpu_begin();

// Liest den Sensor und füllt has_mpu / acc_*_g / gyr_*_dps in rec.
// Ohne erfolgreichen mpu_begin() vorher bleibt rec unverändert.
void mpu_read(telemetry::TelemetryRecord& rec);

#endif // FLIGHT_MPU_SENSOR_H
