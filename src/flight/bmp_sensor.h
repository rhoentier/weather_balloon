// bmp_sensor.h — BMP280 (Temperatur/Druck/Barometer-Höhe) via Adafruit-Lib.
//
// Board-Hardware-Teil (kein nativer Test, analog zu oled.cpp/sd_log.cpp) —
// wird am Heltec V2 verifiziert. I2C-Adresse fest 0x76 (am Board per Scan
// bestätigt; der Adafruit-Default wäre 0x77).
//
// HÖHEN-REFERENZ: bmp_begin() misst den Luftdruck am Startort und nutzt ihn als
// Referenz → alt_baro_m ist die Höhe ÜBER DEM START (Start ≈ 0 m). Fällt der
// Sensor beim Start aus oder misst er unplausibel, bleibt die Standardatmosphäre
// (1013,25 hPa) als Referenz. Für die Flugphasen-Erkennung zählt ohnehin die
// Höhenänderung, nicht der Absolutwert.
//
// HINWEIS: Der verbaute Chip ist ein BMP280 (Chip-ID 0x58, am Board gemessen) —
// KEINE Luftfeuchte. Das als "BME280" verkaufte Modul trägt tatsächlich einen
// BMP280; darum kein humidity-Feld.
#ifndef FLIGHT_BMP_SENSOR_H
#define FLIGHT_BMP_SENSOR_H

#include "record.h"

// I2C muss vorher laufen (Wire.begin(4,15) in main.cpp/setup()).
// Misst zugleich den Startdruck als Höhen-Referenz (siehe oben).
// Rückgabe: true, wenn der Sensor am Bus antwortet (Chip-ID ok).
bool bmp_begin();

// Aktuell verwendeter Referenzdruck (hPa): der beim Start gemessene Startort-
// Druck, oder 1013,25 (Standard), falls der Sensor beim Start fehlte.
float bmp_reference_hpa();

// Liest den Sensor und füllt has_bmp/temp_c/pressure_hpa/alt_baro_m in rec.
// Ohne erfolgreichen bmp_begin() vorher bleibt rec unverändert.
void bmp_read(telemetry::TelemetryRecord& rec);

#endif // FLIGHT_BMP_SENSOR_H
