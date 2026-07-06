// gps_reader.cpp — siehe gps_reader.h.

#include "gps_reader.h"
#include <TinyGPS++.h>

using namespace telemetry;

// Datei-internes GPS-Objekt: hält den geparsten NMEA-Zustand über loop()-Zyklen.
static TinyGPSPlus gps;

// Ein Positions-Fix gilt nur so lange als gültig, wie er FRISCH ist. Grund:
// TinyGPSPlus rastet ein — nach dem ersten Fix bleibt location.isValid()
// dauerhaft true, und lat/lon/FixQuality behalten bei Signalverlust ihren
// letzten Wert (die Lib committet die Position nur bei einem GGA mit Fix).
// Über location.age() (ms seit letztem Update) erzwingen wir, dass ein
// verlorener Fix wieder als „kein Fix" zählt → leere CSV-Felder statt still
// weitergeschriebener, veralteter Position. Bei 1-Hz-GPS reicht ein Fenster,
// das einen ausgefallenen Satz überbrückt.
static const uint32_t GPS_MAX_AGE_MS = 2000;

void gps_feed(Stream& serial) {
    while (serial.available()) {
        gps.encode(static_cast<char>(serial.read()));
    }
}

void gps_fill(TelemetryRecord& r) {
    // Nur ein FRISCHER Fix zählt (siehe GPS_MAX_AGE_MS oben): sonst würde nach
    // Signalverlust die eingerastete alte Position weitergeschrieben.
    bool fresh = gps.location.isValid() && gps.location.age() <= GPS_MAX_AGE_MS;

    // Fix-Qualität immer übernehmen (0 = kein Fix ist ein echter Messwert).
    // ACHTUNG: TinyGPSPlus' FixQuality() liefert das enum Quality, dessen Werte
    // ASCII-Ziffern sind (Invalid='0'=48, GPS='1'=49, ...). Für die rohe 0/1/2..-
    // Zahl im CSV also '0' abziehen — sonst landet 48/49/50 in der Spalte.
    // Ohne frischen Fix ist die Qualität 0 (kein aktueller Fix).
    int q = fresh ? (gps.location.FixQuality() - '0') : 0;
    r.fix_quality = static_cast<uint8_t>(q);
    r.has_fix = fresh && q > 0;

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
    // Fix nur anzeigen, wenn frisch (nicht die eingerastete alte Position) und
    // Qualität > 0. FixQuality() ist ASCII ('0'..), daher gegen '0' vergleichen.
    if (gps.location.isValid() && gps.location.age() <= GPS_MAX_AGE_MS
            && gps.location.FixQuality() > '0')
        return GpsDisp::Fix;
    return GpsDisp::Waiting;                                  // Bytes da, kein (frischer) Fix
}
