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

// Alle Flags mit Default false vorbelegt: main.cpp setzt aktuell nur die
// befüllbaren (sd_ok, bmp_ok, gps, sats, phase); mpu_ok/ds18b20_ok/uv_ok
// bleiben so definiert false, statt bei uninitialisiertem Aggregat zufällig
// „ok"/„--" auf dem OLED zu zeigen (Konvention wie TelemetryRecord in record.h).
struct DisplayState {
    GpsDisp gps        = GpsDisp::Silent;
    uint8_t sats       = 0;
    bool    sd_ok      = false;
    Phase   phase      = Phase::PreFlight;
    bool    bmp_ok     = false;  // BMP280 ok?
    bool    mpu_ok     = false;  // MPU-6050 ok?
    bool    ds18b20_ok = false;  // DS18B20 ok?
    bool    uv_ok      = false;  // GUVA-S12SD (UV) ok?
};

// Anzuzeigende Zeilen. Reihenfolge = Layout-Kontrakt:
//   [0] Titel, [1] GPS, [2] SD, [3] Phase, [4] Sensoren.
std::vector<std::string> status_lines(const DisplayState& s);

} // namespace telemetry

#endif // TELEMETRY_DISPLAY_STATUS_H
