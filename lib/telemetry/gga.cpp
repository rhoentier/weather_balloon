// gga.cpp — NMEA-GGA-Parser (siehe gga.h). Hardware-frei.

#include "gga.h"
#include <vector>
#include <cstdlib>

namespace telemetry {

// Ein Hex-Zeichen -> Wert 0..15, oder -1 wenn kein Hex.
static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

// Prüft NMEA-Rahmen: '$' am Anfang, '*HH' am Ende, XOR-Checksumme korrekt.
// Bei Erfolg: body = Inhalt zwischen '$' und '*'. Rückgabe false bei Formfehler.
static bool check_frame(const std::string& line, std::string& body) {
    if (line.size() < 4) return false;
    if (line[0] != '$') return false;

    // '*' von hinten suchen; danach müssen genau zwei Hex-Ziffern stehen.
    std::size_t star = line.rfind('*');
    if (star == std::string::npos) return false;
    if (star + 2 >= line.size()) return false;  // brauchen zwei Hex-Zeichen
    int hi = hex_val(line[star + 1]);
    int lo = hex_val(line[star + 2]);
    if (hi < 0 || lo < 0) return false;
    int want = (hi << 4) | lo;

    // XOR aller Zeichen zwischen '$' (exkl.) und '*' (exkl.).
    int sum = 0;
    for (std::size_t i = 1; i < star; ++i) sum ^= static_cast<unsigned char>(line[i]);
    if (sum != want) return false;

    body = line.substr(1, star - 1);  // ohne '$' und ohne '*HH'
    return true;
}

// Zerlegt den GGA-Body an Kommata. Leere Felder bleiben als "" erhalten.
static std::vector<std::string> split_fields(const std::string& body) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : body) {
        if (c == ',') { out.push_back(cur); cur.clear(); }
        else { cur.push_back(c); }
    }
    out.push_back(cur);
    return out;
}

// NMEA-Koordinate "ddmm.mmmm" (bzw. "dddmm.mmmm") -> Dezimalgrad.
// deg_digits = 2 für Breite, 3 für Länge. hemi = "N"/"S"/"E"/"W".
// Bei leerem Feld oder Formfehler -> 0.0 (Aufrufer wertet nur bei has_fix aus).
static double nmea_to_decimal(const std::string& field,
                              int deg_digits, const std::string& hemi) {
    if (static_cast<int>(field.size()) < deg_digits) return 0.0;
    double deg = std::strtod(field.substr(0, deg_digits).c_str(), nullptr);
    double min = std::strtod(field.substr(deg_digits).c_str(), nullptr);
    double val = deg + min / 60.0;
    if (hemi == "S" || hemi == "W") val = -val;
    return val;
}

// GGA-Feld [1] "hhmmss(.ss)" -> Stunden/Minuten/Sekunden.
// Gültig nur, wenn die ersten 6 Zeichen Ziffern sind. Nachkomma wird verworfen.
// Rückgabe false -> Feld leer/zu kurz/keine Ziffern (Aufrufer setzt has_utc=false).
static bool parse_utc(const std::string& field,
                      uint8_t& h, uint8_t& m, uint8_t& s) {
    if (field.size() < 6) return false;
    for (int i = 0; i < 6; ++i)
        if (field[i] < '0' || field[i] > '9') return false;
    h = static_cast<uint8_t>((field[0] - '0') * 10 + (field[1] - '0'));
    m = static_cast<uint8_t>((field[2] - '0') * 10 + (field[3] - '0'));
    s = static_cast<uint8_t>((field[4] - '0') * 10 + (field[5] - '0'));
    return true;
}

bool parse_gga(const std::string& line, GpsFix& out) {
    std::string body;
    if (!check_frame(line, body)) return false;

    // Satz-ID: Zeichen 2..4 des body (nach 2 Talker-Zeichen) müssen "GGA" sein.
    // body beginnt z.B. mit "GPGGA," oder "GNGGA,".
    if (body.size() < 5) return false;
    if (body.compare(2, 3, "GGA") != 0) return false;

    // GGA-Felder (Index bezogen auf split_fields(body)):
    //  [0]=Satz-ID  [1]=UTC  [2]=lat  [3]=N/S  [4]=lon  [5]=E/W
    //  [6]=Fix-Qual [7]=sats [8]=HDOP [9]=alt  [10]="M" ...
    auto f = split_fields(body);
    if (f.size() < 10) return false;  // zu kurz -> Formfehler

    // Fix-Qualität: rohen Wert erhalten (0/1/2..), > 0 heißt gültiger Fix.
    long qual = std::strtol(f[6].c_str(), nullptr, 10);
    out.fix_quality = static_cast<uint8_t>(qual);
    out.has_fix = (qual > 0);

    // Satelliten + Höhe immer aus den Feldern lesen (auch ohne Fix harmlos).
    out.sats = static_cast<uint8_t>(std::strtoul(f[7].c_str(), nullptr, 10));
    out.alt_gps_m = static_cast<float>(std::strtod(f[9].c_str(), nullptr));

    // Koordinaten: Breite ddmm.mmmm (2 Grad-Stellen), Länge dddmm.mmmm (3).
    out.lat = nmea_to_decimal(f[2], 2, f[3]);
    out.lon = nmea_to_decimal(f[4], 3, f[5]);

    // UTC aus Feld [1] (hhmmss), unabhängig vom Positions-Fix.
    out.has_utc = parse_utc(f[1], out.utc_h, out.utc_min, out.utc_s);

    return true;
}

} // namespace telemetry
