// gga.h — NMEA-GGA-Parser: eine GGA-Zeile -> GpsFix.
//
// Hardware-frei (kein <Arduino.h>) -> nativ testbar. Kennt NUR GPS, nicht das
// Telemetrie-/CSV-Format. Der Aufrufer (src/flight) kopiert die Felder danach
// in den TelemetryRecord.
//
// GGA liefert alle GPS-Felder des Records: Fix, lat, lon, Höhe, Satelliten.
// NMEA-Koordinaten sind ddmm.mmmm (Grad+Minuten verklebt), NICHT Dezimalgrad!

#ifndef TELEMETRY_GGA_H
#define TELEMETRY_GGA_H

#include <cstdint>
#include <string>

namespace telemetry {

struct GpsFix {
    bool    has_fix = false;   // true = gültiger Satellitenfix (Fix-Qualität > 0)
    uint8_t fix_quality = 0;   // rohes GGA-Feld [6]: 0=kein Fix, 1=GPS, 2=DGPS ...
    double  lat = 0.0;         // Dezimalgrad; N/O positiv, S/W negativ
    double  lon = 0.0;
    float   alt_gps_m = 0.0f;  // Höhe über Meer (GGA-Feld, "M")
    uint8_t sats = 0;          // Anzahl genutzter Satelliten

    // --- UTC-Uhrzeit aus GGA-Feld [1] (hhmmss), unabhängig vom Fix ---
    bool    has_utc = false;   // true = gültige Uhrzeit geparst
    uint8_t utc_h = 0;         // 0..23
    uint8_t utc_min = 0;       // 0..59
    uint8_t utc_s = 0;         // 0..59 (Nachkommastellen verworfen)
};

// return false -> keine brauchbare GGA-Zeile (kein $..GGA, kaputte Checksumme,
//                 Formfehler). out bleibt UNBERÜHRT.
// return true  -> wohlgeformter GGA-Satz mit gültiger Checksumme.
//                 out.has_fix = (Fix-Qualität > 0). Ohne Fix sind lat/lon/alt/
//                 sats undefiniert (Aufrufer schreibt dann leere CSV-Felder).
bool parse_gga(const std::string& line, GpsFix& out);

} // namespace telemetry

#endif // TELEMETRY_GGA_H
