// gps_reader.cpp — siehe gps_reader.h.

#include "gps_reader.h"
#include <TinyGPS++.h>

using namespace telemetry;

// Datei-internes GPS-Objekt: hält den geparsten NMEA-Zustand über loop()-Zyklen.
static TinyGPSPlus gps;

void gps_feed(Stream& serial) {
    while (serial.available()) {
        gps.encode(static_cast<char>(serial.read()));
    }
}

void gps_fill(TelemetryRecord& r) {
    // Fix-Qualität immer übernehmen (0 = kein Fix ist ein echter Messwert).
    r.fix_quality = static_cast<uint8_t>(gps.location.FixQuality());
    r.has_fix = gps.location.isValid() && gps.location.FixQuality() > 0;

    // Satelliten: nur bei gültigem Feld übernehmen, sonst 0.
    r.sats = gps.satellites.isValid()
                 ? static_cast<uint8_t>(gps.satellites.value())
                 : 0;

    if (r.has_fix) {
        r.lat = gps.location.lat();
        r.lon = gps.location.lng();
    }
    if (gps.altitude.isValid()) {
        r.alt_gps_m = static_cast<float>(gps.altitude.meters());
    }

    // UTC-Zeit unabhängig vom Positions-Fix.
    r.has_utc = gps.time.isValid();
    if (r.has_utc) {
        r.utc_h   = static_cast<uint8_t>(gps.time.hour());
        r.utc_min = static_cast<uint8_t>(gps.time.minute());
        r.utc_s   = static_cast<uint8_t>(gps.time.second());
    }
}

GpsDisp gps_display_state() {
    if (gps.charsProcessed() == 0) return GpsDisp::Silent;   // Modul stumm
    if (gps.location.isValid() && gps.location.FixQuality() > 0)
        return GpsDisp::Fix;
    return GpsDisp::Waiting;                                  // Bytes da, kein Fix
}
