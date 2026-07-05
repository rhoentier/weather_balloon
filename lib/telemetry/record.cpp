// record.cpp — Telemetrie-/CSV-Format (siehe record.h). Hardware-frei.

#include "record.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace telemetry {

// ---- Hilfen --------------------------------------------------------------

// Float/Double mit fester Präzision als String (z.B. "48.137235").
static std::string num(double v, int decimals) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return std::string(buf);
}

// Zerlegt eine CSV-Zeile in Felder (leere Felder bleiben als "" erhalten).
static std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == ',') { out.push_back(cur); cur.clear(); }
        else if (c != '\r' && c != '\n') { cur.push_back(c); }
    }
    out.push_back(cur);
    return out;
}

// ---- Schnittstelle -------------------------------------------------------
// REIHENFOLGE der Spalten muss in header / row / parse identisch sein.

std::string csv_header() {
    return "t_ms,utc,phase,fix_q,lat,lon,alt_gps_m,sats,temp_c,pressure_hpa,alt_baro_m,humidity_pct";
}

std::string csv_row(const TelemetryRecord& r) {
    std::string s;
    s += std::to_string(r.t_ms);
    s += ',';
    // UTC: hh:mm:ss mit führenden Nullen, sonst leeres Feld.
    if (r.has_utc) {
        char buf[9];
        std::snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
                      r.utc_h, r.utc_min, r.utc_s);
        s += buf;
    }
    s += ',';
    s += to_string(r.phase);
    s += ',';
    // Fix-Qualität immer schreiben (0 = kein Fix ist ein echter Messwert).
    s += std::to_string(r.fix_quality);
    s += ',';
    // GPS: nur bei Fix Werte, sonst leere Felder.
    if (r.has_fix) {
        s += num(r.lat, 6);       s += ',';
        s += num(r.lon, 6);       s += ',';
        s += num(r.alt_gps_m, 1); s += ',';
        s += std::to_string(r.sats);
    } else {
        s += ",,,";  // lat, lon, alt_gps_m, sats alle leer
    }
    s += ',';
    // BME280: nur bei bestücktem Sensor Werte, sonst vier leere Felder.
    if (r.has_bme) {
        s += num(r.temp_c, 2);       s += ',';
        s += num(r.pressure_hpa, 2); s += ',';
        s += num(r.alt_baro_m, 2);   s += ',';
        s += num(r.humidity_pct, 2);
    } else {
        s += ",,,";  // temp_c, pressure_hpa, alt_baro_m, humidity_pct alle leer
    }
    return s;
}

bool parse_csv_row(const std::string& line, TelemetryRecord& out) {
    if (line.empty()) return false;
    auto f = split(line);
    if (f.size() != 12) return false;          // Spaltenzahl muss passen
    if (f[0].empty()) return false;           // t_ms ist Pflicht

    out = TelemetryRecord{};                  // sauber zurücksetzen
    out.t_ms = static_cast<uint32_t>(std::strtoul(f[0].c_str(), nullptr, 10));

    // UTC (Feld [1]): "hh:mm:ss" -> Werte; leeres Feld -> has_utc bleibt false.
    if (!f[1].empty()) {
        unsigned h = 0, m = 0, sec = 0;
        if (std::sscanf(f[1].c_str(), "%u:%u:%u", &h, &m, &sec) == 3) {
            out.has_utc = true;
            out.utc_h = static_cast<uint8_t>(h);
            out.utc_min = static_cast<uint8_t>(m);
            out.utc_s = static_cast<uint8_t>(sec);
        }
    }

    // Phase aus Label (Feld [2]).
    const std::string& p = f[2];
    if      (p == "PREFLIGHT") out.phase = Phase::PreFlight;
    else if (p == "ASCENT")    out.phase = Phase::Ascent;
    else if (p == "DESCENT")   out.phase = Phase::Descent;
    else if (p == "LANDED")    out.phase = Phase::Landed;

    // Fix-Qualität (Feld [3]): leeres Feld -> 0.
    if (!f[3].empty()) out.fix_quality = static_cast<uint8_t>(std::strtoul(f[3].c_str(), nullptr, 10));

    // GPS: Fix gilt als vorhanden, wenn lat/lon (Felder [4]/[5]) befüllt sind.
    if (!f[4].empty() && !f[5].empty()) {
        out.has_fix = true;
        out.lat = std::strtod(f[4].c_str(), nullptr);
        out.lon = std::strtod(f[5].c_str(), nullptr);
        if (!f[6].empty()) out.alt_gps_m = static_cast<float>(std::strtod(f[6].c_str(), nullptr));
        if (!f[7].empty()) out.sats = static_cast<uint8_t>(std::strtoul(f[7].c_str(), nullptr, 10));
    }
    // BME280 (Felder [8]/[9]/[10]/[11]): gilt als vorhanden, wenn temp_c befüllt ist.
    if (!f[8].empty()) {
        out.has_bme = true;
        out.temp_c = static_cast<float>(std::strtod(f[8].c_str(), nullptr));
        if (!f[9].empty())  out.pressure_hpa = static_cast<float>(std::strtod(f[9].c_str(), nullptr));
        if (!f[10].empty()) out.alt_baro_m   = static_cast<float>(std::strtod(f[10].c_str(), nullptr));
        if (!f[11].empty()) out.humidity_pct = static_cast<float>(std::strtod(f[11].c_str(), nullptr));
    }
    return true;
}

} // namespace telemetry
