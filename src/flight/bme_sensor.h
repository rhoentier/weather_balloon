// bme_sensor.h — BME280 (Temperatur/Druck/Höhe/Feuchte) via Adafruit-Bibliothek.
//
// Board-Hardware-Teil (kein nativer Test, analog zu oled.cpp/sd_log.cpp) —
// wird am Heltec V2 verifiziert. I2C-Adresse fest 0x76 (typischer Default
// günstiger BME280-Breakouts). QNH fest 1013,25 hPa (Standardatmosphäre,
// keine Boden-Kalibrierung — YAGNI): Höhe ist damit eine RELATIVE Größe,
// für die Flugphasen-Erkennung reicht die Änderung, nicht der Absolutwert.

#ifndef FLIGHT_BME_SENSOR_H
#define FLIGHT_BME_SENSOR_H

#include "record.h"

// I2C muss vorher laufen (z.B. über oled_begin(), das denselben Bus startet).
// Rückgabe: true, wenn der Sensor am Bus antwortet (Chip-ID ok).
bool bme_begin();

// Liest den Sensor und füllt has_bme/temp_c/pressure_hpa/alt_baro_m/
// humidity_pct in rec. Ohne erfolgreichen bme_begin() vorher bleibt rec
// unverändert (has_bme bleibt, was es vorher war).
void bme_read(telemetry::TelemetryRecord& rec);

#endif // FLIGHT_BME_SENSOR_H
