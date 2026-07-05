// gps_reader.h — GPS-Auslese über TinyGPSPlus (Hardware-Teil, Arduino-gebunden).
//
// Kapselt das TinyGPSPlus-Objekt. main.cpp füttert den UART-Strom (gps_feed),
// liest bei Bedarf den aktuellen Zustand in den TelemetryRecord (gps_fill) und
// fragt den Boden-Check-Zustand fürs OLED ab (gps_display_state).
//
// Ersetzt den früheren Eigenbau (lib/telemetry/gga + line_assembler): TinyGPSPlus
// sammelt den Byte-Strom UND parst NMEA in einem Schritt.

#ifndef FLIGHT_GPS_READER_H
#define FLIGHT_GPS_READER_H

#include <Arduino.h>
#include "record.h"          // telemetry::TelemetryRecord
#include "display_status.h"  // telemetry::GpsDisp

// Liest alle aktuell verfügbaren UART-Bytes von `gps` und füttert sie an
// TinyGPSPlus. Nicht-blockierend: verarbeitet nur, was schon da ist.
void gps_feed(Stream& gps);

// Schreibt den aktuellen GPS-Zustand in r: lat, lon, alt_gps_m, sats,
// fix_quality, has_fix, utc_* und has_utc. Ohne Fix bleiben lat/lon/alt
// unangetastet und has_fix=false (Aufrufer schreibt dann leere CSV-Felder).
void gps_fill(telemetry::TelemetryRecord& r);

// Boden-Check-Zustand fürs OLED:
//   Silent  = seit Boot kein einziges Byte vom GPS empfangen
//   Waiting = Bytes kommen an, aber (noch) kein gültiger Fix
//   Fix     = gültiger Positions-Fix vorhanden
telemetry::GpsDisp gps_display_state();

#endif // FLIGHT_GPS_READER_H
