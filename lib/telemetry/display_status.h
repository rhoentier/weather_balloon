// display_status.h — OLED-Statuszeilen (hardware-frei, nativ getestet).
//
// BEWUSST ohne <Arduino.h>/U8g2: nur Textformatierung. Die Firmware baut den
// DisplayState und lässt hier die Zeilen erzeugen; das Zeichnen liegt in
// src/flight/oled. Zweck: „Startklar?"-Check am Boden (welche Komponenten laufen).
#ifndef TELEMETRY_DISPLAY_STATUS_H
#define TELEMETRY_DISPLAY_STATUS_H

#include <cstdint>
#include <string>
#include <vector>
#include "flight_phase.h"   // Phase, to_string(Phase)

namespace telemetry {

// GPS-Stufe fürs Display (zweistufig, siehe Spec):
//   Silent  = seit Boot noch nie GGA gesehen
//   Waiting = GGA kommt, aber kein Fix
//   Fix     = Fix vorhanden (sats gültig)
enum class GpsDisp { Silent, Waiting, Fix };

struct DisplayState {
    GpsDisp gps;
    uint8_t sats;
    bool    sd_ok;
    Phase   phase;
    bool    bme_ok;      // BME280 ok?
    bool    mpu_ok;      // MPU-6050 ok?
    bool    ds18b20_ok;  // DS18B20 ok?
    bool    uv_ok;       // GUVA-S12SD (UV) ok?
};

// Anzuzeigende Zeilen. Reihenfolge = Layout-Kontrakt:
//   [0] Titel, [1] GPS, [2] SD, [3] Phase, [4] Sensoren.
std::vector<std::string> status_lines(const DisplayState& s);

} // namespace telemetry

#endif // TELEMETRY_DISPLAY_STATUS_H
