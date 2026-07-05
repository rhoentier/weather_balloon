// record.h — Telemetrie-/Log-Datensatz: EINE Schnittstelle für beides.
//
// Dasselbe Format geht (a) als CSV-Zeile auf die microSD und (b) als
// LoRa-Telemetrie zur Bodenstation. Beide Seiten nutzen denselben Code →
// SD-Log und Empfangs-Log sind hinterher direkt vergleichbar (Konzept §7.2).
// Direkt in Excel/Python lesbar (CSV).
//
// Hardware-frei (kein <Arduino.h>) → nativ testbar.
//
// ERWEITERN nach YAGNI: Wenn ein Sensor dazukommt, genau drei Stellen:
//   1) Feld in TelemetryRecord ergänzen
//   2) Spaltenname in csv_header()
//   3) Wert in csv_row() schreiben + in parse_csv_row() lesen
// Fehlende Werte (kein GPS-Fix, Sensor nicht bestückt) = LEERES Feld.

#ifndef TELEMETRY_RECORD_H
#define TELEMETRY_RECORD_H

#include <cstdint>
#include <string>
#include "flight_phase.h"

namespace telemetry {

// Ein Messzyklus. Jedes optionale Feld hat ein `has_*`-Flag: false → das Feld
// wird als leere CSV-Spalte geschrieben (nativ als NaN/leer lesbar).
struct TelemetryRecord {
    uint32_t t_ms = 0;                  // monotone Bordzeit (immer vorhanden)
    Phase    phase = Phase::PreFlight;  // aktuelle Flugphase (immer vorhanden)

    // --- GPS-UTC-Uhrzeit (aus GGA), unabhängig vom Positions-Fix ---
    bool    has_utc = false;
    uint8_t utc_h = 0;
    uint8_t utc_min = 0;
    uint8_t utc_s = 0;

    // --- GPS (NEO-6M) ---
    bool    has_fix = false;
    uint8_t fix_quality = 0;  // rohe GGA-Fix-Qualität: 0=kein Fix, 1=GPS, 2=DGPS ...
    double lat = 0.0;        // Grad
    double lon = 0.0;        // Grad
    float  alt_gps_m = 0.0f; // geometrische Höhe
    uint8_t sats = 0;        // Anzahl Satelliten

    // --- BMP280 (Temperatur/Druck/Barometer-Höhe), ein gemeinsames Flag ---
    bool  has_bmp = false;
    float temp_c = 0.0f;        // °C
    float pressure_hpa = 0.0f;  // hPa
    float alt_baro_m = 0.0f;    // barometrische Höhe (m), aus Druck + QNH
};

// Kopfzeile mit Spaltennamen (ohne Zeilenende). Reihenfolge == csv_row().
std::string csv_header();

// Eine Datenzeile (ohne Zeilenende). Fehlende Werte → leeres Feld.
std::string csv_row(const TelemetryRecord& r);

// Parst eine mit csv_row() erzeugte Zeile zurück in einen Record.
// Rückgabe: true bei Erfolg. Leere Felder → has_*-Flag bleibt false.
bool parse_csv_row(const std::string& line, TelemetryRecord& out);

} // namespace telemetry

#endif // TELEMETRY_RECORD_H
