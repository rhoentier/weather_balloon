# NMEA-GGA-Parser Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ein hardware-freier Parser, der eine NMEA-GGA-Zeile in ein `GpsFix`-Struct verwandelt (`lib/telemetry/gga`), test-getrieben und nativ getestet.

**Architecture:** Reine C++17-Logik im `telemetry`-Namespace, kein `<Arduino.h>`. Eine Funktion `parse_gga(line, GpsFix&)` prüft Talker-Präfix, Checksumme und Feldform, rechnet NMEA-`ddmm.mmmm` in Dezimalgrad um und füllt bei Erfolg das Struct. `src/flight` kopiert später die Felder in den `TelemetryRecord` — nicht Teil dieses Plans.

**Tech Stack:** C++17, PlatformIO `native`-Env, Unity-Test-Framework.

## Global Constraints

- Sprache: Deutsch (Kommentare, Commits, Doku).
- Hardware-frei: kein `<Arduino.h>`, kein Board-Code in `lib/telemetry/`.
- Namespace `telemetry`, Include-Guard `TELEMETRY_GGA_H`.
- TDD verbindlich: erst Test, RED sehen (aus richtigem Grund), dann minimaler Code (GREEN), dann aufräumen.
- Gegen bekannt-korrekte Referenzsequenzen testen, nicht gegen eigene Annahme.
- Ein eigener Test-Ordner `test/test_gga/` mit eigenem `main()` (`UNITY_BEGIN/END`) — sonst Linker-Kollision mit anderen Tests.
- Double-Asserts sind im native-Env aktiviert (`-D UNITY_INCLUDE_DOUBLE`), da lat/lon `double` sind.
- Test-Kommando: `export PATH="$PATH:$HOME/.platformio/penv/bin"` dann `pio test -e native`.
- YAGNI: nur GGA, kein anderes Satzformat, kein Zeilenpuffer/Streaming, keine UTC/Speed/HDOP.
- Projekt ist **kein** Git-Repo → Commit-Schritte entfallen; stattdessen am Ende jeder Task Tests laufen lassen.

---

### Task 1: `GpsFix`-Struct + `parse_gga`-Signatur (Header)

**Files:**
- Create: `lib/telemetry/gga.h`
- Test: `test/test_gga/test_gga.cpp` (nur Kompilier-/Signatur-Smoke-Test)

**Interfaces:**
- Consumes: nichts.
- Produces:
  - `struct telemetry::GpsFix { bool has_fix; double lat, lon; float alt_gps_m; uint8_t sats; };`
  - `bool telemetry::parse_gga(const std::string& line, GpsFix& out);`

- [ ] **Step 1: Header schreiben**

`lib/telemetry/gga.h`:
```cpp
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
    double  lat = 0.0;         // Dezimalgrad; N/O positiv, S/W negativ
    double  lon = 0.0;
    float   alt_gps_m = 0.0f;  // Höhe über Meer (GGA-Feld, "M")
    uint8_t sats = 0;          // Anzahl genutzter Satelliten
};

// return false -> keine brauchbare GGA-Zeile (kein $..GGA, kaputte Checksumme,
//                 Formfehler). out bleibt UNBERÜHRT.
// return true  -> wohlgeformter GGA-Satz mit gültiger Checksumme.
//                 out.has_fix = (Fix-Qualität > 0). Ohne Fix sind lat/lon/alt/
//                 sats undefiniert (Aufrufer schreibt dann leere CSV-Felder).
bool parse_gga(const std::string& line, GpsFix& out);

} // namespace telemetry

#endif // TELEMETRY_GGA_H
```

- [ ] **Step 2: Minimaler Test-Rahmen, der die Signatur nutzt**

`test/test_gga/test_gga.cpp`:
```cpp
// test_gga.cpp — native Tests für den NMEA-GGA-Parser.
//   pio test -e native
//
// Referenzsätze mit echten, nachgerechneten Werten. NMEA-Koordinaten sind
// ddmm.mmmm -> Dezimalgrad = dd + mm.mmmm/60.

#include <unity.h>
#include "gga.h"
#include <string>

using namespace telemetry;

void setUp() {}
void tearDown() {}

// Platzhalter, bis die echten Tests folgen (Task 2+).
void test_signature_smoke() {
    GpsFix fix;
    bool ok = parse_gga("", fix);
    TEST_ASSERT_FALSE(ok);  // leere Zeile ist nie brauchbar
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_signature_smoke);
    return UNITY_END();
}
```

- [ ] **Step 3: Test laufen lassen — muss FEHLSCHLAGEN (Linker: parse_gga undefiniert)**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && pio test -e native`
Expected: FAIL — Linker-Fehler „undefined reference to `telemetry::parse_gga`" (die Implementierung fehlt noch). Das ist das erwartete RED.

- [ ] **Step 4: Minimale Implementierung, damit gelinkt wird und der Smoke-Test grün ist**

`lib/telemetry/gga.cpp`:
```cpp
// gga.cpp — NMEA-GGA-Parser (siehe gga.h). Hardware-frei.

#include "gga.h"

namespace telemetry {

bool parse_gga(const std::string& line, GpsFix& out) {
    (void)out;
    if (line.empty()) return false;
    return false;  // vorerst: nichts als brauchbar erkennen (Task 2 füllt aus)
}

} // namespace telemetry
```

- [ ] **Step 5: Test laufen lassen — muss BESTEHEN**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && pio test -e native`
Expected: PASS (`test_signature_smoke`), alle bisherigen Tests weiterhin grün.

---

### Task 2: Checksumme + Talker-Erkennung (nur Gültigkeit, noch keine Felder)

**Files:**
- Modify: `lib/telemetry/gga.cpp`
- Test: `test/test_gga/test_gga.cpp`

**Interfaces:**
- Consumes: `parse_gga`, `GpsFix` aus Task 1.
- Produces: `parse_gga` gibt `true` nur für `$xxGGA`-Zeilen mit gültiger `*HH`-Checksumme zurück; `has_fix`/Felder noch nicht befüllt.

- [ ] **Step 1: Failing Tests für Gültigkeit schreiben**

In `test/test_gga/test_gga.cpp` `test_signature_smoke` ersetzen durch:
```cpp
// Referenz-GGA mit gültiger Checksumme (*47 stimmt für diese Zeile).
static const char* GGA_VALID =
    "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47";

// Gültige Zeile -> true.
void test_valid_gga_accepted() {
    GpsFix fix;
    TEST_ASSERT_TRUE(parse_gga(GGA_VALID, fix));
}

// Verfälschte Checksumme (*48 statt *47) -> false.
void test_bad_checksum_rejected() {
    GpsFix fix;
    std::string bad = GGA_VALID;
    bad[bad.size() - 1] = '8';  // *47 -> *48
    TEST_ASSERT_FALSE(parse_gga(bad, fix));
}

// Nicht-GGA-Satz (RMC) -> false.
void test_non_gga_rejected() {
    GpsFix fix;
    // RMC-Zeile mit gültiger Checksumme, aber kein GGA.
    TEST_ASSERT_FALSE(parse_gga(
        "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A",
        fix));
}

// Talker-Variante $GNGGA (Multi-GNSS) wird als GGA erkannt.
void test_gnss_talker_accepted() {
    GpsFix fix;
    // Gleiche Nutzlast wie GGA_VALID, aber Talker GN statt GP -> andere Checksumme.
    // Checksumme für "GNGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,," = 0x59
    TEST_ASSERT_TRUE(parse_gga(
        "$GNGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*59",
        fix));
}

// Leere Zeile -> false, kein Absturz.
void test_empty_rejected() {
    GpsFix fix;
    TEST_ASSERT_FALSE(parse_gga("", fix));
}
```

Und `main()` anpassen:
```cpp
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_valid_gga_accepted);
    RUN_TEST(test_bad_checksum_rejected);
    RUN_TEST(test_non_gga_rejected);
    RUN_TEST(test_gnss_talker_accepted);
    RUN_TEST(test_empty_rejected);
    return UNITY_END();
}
```

> Hinweis für den Implementierer: Die Checksummen `*47` und `*59` sind gegen die
> XOR-Definition nachzurechnen (XOR aller Zeichen zwischen `$` und `*`). Falls
> ein Referenzwert nicht stimmt, ist der Referenzwert zu korrigieren — NICHT die
> Prüf-Logik daran anzupassen. Gegen die Definition testen, nicht gegen Annahme.

- [ ] **Step 2: Tests laufen lassen — müssen FEHLSCHLAGEN**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && pio test -e native`
Expected: `test_valid_gga_accepted` und `test_gnss_talker_accepted` FAIL (parse_gga gibt immer false). Die übrigen (bad checksum / non-gga / empty) sind zufällig schon grün — das ist ok, sie sichern das Verhalten ab.

- [ ] **Step 3: Checksumme + Talker-Erkennung implementieren**

`lib/telemetry/gga.cpp` ersetzen durch:
```cpp
// gga.cpp — NMEA-GGA-Parser (siehe gga.h). Hardware-frei.

#include "gga.h"
#include <cctype>

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

bool parse_gga(const std::string& line, GpsFix& out) {
    std::string body;
    if (!check_frame(line, body)) return false;

    // Satz-ID: Zeichen 2..4 des body (nach 2 Talker-Zeichen) müssen "GGA" sein.
    // body beginnt z.B. mit "GPGGA," oder "GNGGA,".
    if (body.size() < 5) return false;
    if (body.compare(2, 3, "GGA") != 0) return false;

    return true;  // Felder folgen in Task 3
}

} // namespace telemetry
```

- [ ] **Step 4: Tests laufen lassen — müssen BESTEHEN**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && pio test -e native`
Expected: alle 5 GGA-Tests PASS. Falls `test_valid_gga_accepted`/`test_gnss_talker_accepted` fehlschlagen: Referenz-Checksumme im Test gegen die XOR-Definition nachrechnen und korrigieren.

---

### Task 3: Felder parsen — Fix-Qualität, Höhe, Satelliten

**Files:**
- Modify: `lib/telemetry/gga.cpp`
- Test: `test/test_gga/test_gga.cpp`

**Interfaces:**
- Consumes: `parse_gga`, `check_frame` aus Task 2.
- Produces: `parse_gga` füllt bei gültigem Satz `out.has_fix` (Fix-Qualität > 0), `out.alt_gps_m`, `out.sats`. lat/lon folgen in Task 4.

- [ ] **Step 1: Failing Tests für Fix/Höhe/Sats schreiben**

In `test/test_gga/test_gga.cpp` ergänzen:
```cpp
// Gültiger Fix (Qualität 1) -> has_fix true, Höhe + Sats korrekt.
void test_fields_with_fix() {
    GpsFix fix;
    TEST_ASSERT_TRUE(parse_gga(GGA_VALID, fix));
    TEST_ASSERT_TRUE(fix.has_fix);
    TEST_ASSERT_EQUAL_UINT8(8, fix.sats);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 545.4f, fix.alt_gps_m);
}

// Fix-Qualität 0 -> gültige Zeile (true), aber has_fix false.
void test_quality_zero_no_fix() {
    GpsFix fix;
    // Fix-Feld 1 -> 0 gesetzt; Checksumme neu: 0x76.
    TEST_ASSERT_TRUE(parse_gga(
        "$GPGGA,123519,4807.038,N,01131.000,E,0,08,0.9,545.4,M,46.9,M,,*76",
        fix));
    TEST_ASSERT_FALSE(fix.has_fix);
}
```
Und in `main()`:
```cpp
    RUN_TEST(test_fields_with_fix);
    RUN_TEST(test_quality_zero_no_fix);
```

- [ ] **Step 2: Tests laufen lassen — müssen FEHLSCHLAGEN**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && pio test -e native`
Expected: `test_fields_with_fix` FAIL (has_fix/sats/alt noch nicht gesetzt). `test_quality_zero_no_fix`: der `true`-Teil grün, aber `has_fix` ist Default false → evtl. schon grün; das ist ok.

- [ ] **Step 3: Feld-Zerlegung + Fix/Höhe/Sats implementieren**

In `lib/telemetry/gga.cpp` oben eine Split-Hilfe ergänzen (nach `check_frame`):
```cpp
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
```
Include ergänzen (oben bei den anderen):
```cpp
#include <vector>
#include <cstdlib>
```
Und `parse_gga` erweitern — das abschließende `return true;` aus Task 2 ersetzen durch:
```cpp
    // GGA-Felder (Index bezogen auf split_fields(body)):
    //  [0]=Satz-ID  [1]=UTC  [2]=lat  [3]=N/S  [4]=lon  [5]=E/W
    //  [6]=Fix-Qual [7]=sats [8]=HDOP [9]=alt  [10]="M" ...
    auto f = split_fields(body);
    if (f.size() < 10) return false;  // zu kurz -> Formfehler

    // Fix-Qualität: > 0 heißt gültiger Fix.
    long qual = std::strtol(f[6].c_str(), nullptr, 10);
    out.has_fix = (qual > 0);

    // Satelliten + Höhe immer aus den Feldern lesen (auch ohne Fix harmlos).
    out.sats = static_cast<uint8_t>(std::strtoul(f[7].c_str(), nullptr, 10));
    out.alt_gps_m = static_cast<float>(std::strtod(f[9].c_str(), nullptr));

    return true;
```

- [ ] **Step 4: Tests laufen lassen — müssen BESTEHEN**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && pio test -e native`
Expected: alle Tests PASS.

---

### Task 4: Koordinaten `ddmm.mmmm` → Dezimalgrad (mit Vorzeichen)

**Files:**
- Modify: `lib/telemetry/gga.cpp`
- Test: `test/test_gga/test_gga.cpp`

**Interfaces:**
- Consumes: `parse_gga`, `split_fields` aus Task 3.
- Produces: `parse_gga` füllt `out.lat`/`out.lon` in Dezimalgrad; S/W negativ.

- [ ] **Step 1: Failing Tests für Koordinaten schreiben**

In `test/test_gga/test_gga.cpp` ergänzen:
```cpp
// 4807.038,N -> 48 + 07.038/60 = 48.1173 ; 01131.000,E -> 11 + 31/60 = 11.5167.
void test_coords_north_east() {
    GpsFix fix;
    TEST_ASSERT_TRUE(parse_gga(GGA_VALID, fix));
    TEST_ASSERT_DOUBLE_WITHIN(1e-4, 48.1173, fix.lat);
    TEST_ASSERT_DOUBLE_WITHIN(1e-4, 11.5167, fix.lon);
}

// Süd/West -> negative Vorzeichen. Checksumme für diese Zeile: 0x45.
void test_coords_south_west() {
    GpsFix fix;
    TEST_ASSERT_TRUE(parse_gga(
        "$GPGGA,123519,4807.038,S,01131.000,W,1,08,0.9,545.4,M,46.9,M,,*45",
        fix));
    TEST_ASSERT_DOUBLE_WITHIN(1e-4, -48.1173, fix.lat);
    TEST_ASSERT_DOUBLE_WITHIN(1e-4, -11.5167, fix.lon);
}
```
Und in `main()`:
```cpp
    RUN_TEST(test_coords_north_east);
    RUN_TEST(test_coords_south_west);
```

- [ ] **Step 2: Tests laufen lassen — müssen FEHLSCHLAGEN**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && pio test -e native`
Expected: beide FAIL (lat/lon noch 0.0). Erwartetes RED.

- [ ] **Step 3: ddmm→dezimal-Umrechnung implementieren**

In `lib/telemetry/gga.cpp` eine Hilfe ergänzen (nach `split_fields`):
```cpp
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
```
Und in `parse_gga` vor `return true;` ergänzen:
```cpp
    // Koordinaten: Breite ddmm.mmmm (2 Grad-Stellen), Länge dddmm.mmmm (3).
    out.lat = nmea_to_decimal(f[2], 2, f[3]);
    out.lon = nmea_to_decimal(f[4], 3, f[5]);
```

- [ ] **Step 4: Tests laufen lassen — müssen BESTEHEN**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && pio test -e native`
Expected: alle Tests PASS.

---

### Task 5: Robustheit gegen abgeschnittene Zeilen + Aufräumen

**Files:**
- Modify: `lib/telemetry/gga.cpp` (nur falls nötig)
- Test: `test/test_gga/test_gga.cpp`

**Interfaces:**
- Consumes: fertiges `parse_gga`.
- Produces: keine neuen Symbole; nur zusätzliche Absicherung.

- [ ] **Step 1: Failing/Absicherungs-Test für abgeschnittene Zeile schreiben**

In `test/test_gga/test_gga.cpp` ergänzen:
```cpp
// Mitten abgeschnittene GGA-Zeile (kein '*HH') -> false, kein Absturz.
void test_truncated_line_rejected() {
    GpsFix fix;
    TEST_ASSERT_FALSE(parse_gga("$GPGGA,123519,4807.03", fix));
}

// GGA mit gültiger Checksumme, aber zu wenig Feldern -> false.
void test_too_few_fields_rejected() {
    GpsFix fix;
    // "GPGGA,123519,4807.038,N" XOR = 0x1F.
    TEST_ASSERT_FALSE(parse_gga("$GPGGA,123519,4807.038,N*1F", fix));
}
```
Und in `main()`:
```cpp
    RUN_TEST(test_truncated_line_rejected);
    RUN_TEST(test_too_few_fields_rejected);
```

- [ ] **Step 2: Tests laufen lassen**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && pio test -e native`
Expected: `test_truncated_line_rejected` PASS (kein `*` → `check_frame` gibt false). `test_too_few_fields_rejected` PASS (die `f.size() < 10`-Prüfung greift). Falls einer FAILt: `check_frame`/Feldzahl-Prüfung entsprechend korrigieren, bis grün. Referenz-Checksummen bei FAIL gegen XOR-Definition nachrechnen.

- [ ] **Step 3: Native-Env um den neuen Baustein prüfen**

`build_src_filter = +<../lib/telemetry/>` in `platformio.ini` erfasst `gga.cpp` bereits automatisch (ganzer Ordner). Keine Änderung nötig — nur verifizieren, dass `gga.cpp` mitkompiliert wird (erscheint in der Build-Ausgabe).

- [ ] **Step 4: Gesamte native Test-Suite laufen lassen — alles grün**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && pio test -e native`
Expected: alle Tests aller Ordner (test_flight_phase, test_record, test_ubx, test_gga) PASS.

---

### Task 6: `docs/software-flow.md` aktualisieren

**Files:**
- Modify: `docs/software-flow.md`

**Interfaces:**
- Consumes: nichts.
- Produces: aktualisierte Doku (kein Code).

- [ ] **Step 1: §6-Tabelle aktualisieren**

Zeile „NMEA-Parser (GPS-Rohdaten → Record)" von `⬜ nächster Logik-Baustein` auf `✅ nativ getestet` setzen; Ort auf `lib/telemetry/gga`.

- [ ] **Step 2: `loop()`-Diagramm (§3) aktualisieren**

Den Knoten `READ_GPS` von `⬜ NMEA-Parser → lat/lon/alt/sats` auf `✅ parse_gga → GpsFix` ändern und im `style`-Block von grau (`#f8f9fa`) auf grün (`#d4edda`) umstellen.

- [ ] **Step 3: Schlusssatz in §6 anpassen**

Der Absatz „Der naheliegendste nächste rein testbare Schritt ist der NMEA-Parser…" ist erledigt → auf den nun nächsten testbaren Baustein verweisen (z.B. Sensor-Umrechnungen in `lib/`, oder Hinweis, dass als Nächstes ein `src/flight`-Board-Schritt ansteht: GGA-Zeilen aus UART lesen und in den Record kopieren).

- [ ] **Step 4: Sichtprüfung**

Diagramm-Marker, Tabelle und Text konsistent — kein Baustein ist grün markiert, der nicht getestet ist.

---

## Self-Review

**Spec coverage:**
- Nur GGA ✅ (Task 2 Talker-Erkennung, kein anderer Satz).
- Checksumme prüfen ✅ (Task 2).
- `GpsFix`-Struct + `bool`-API ✅ (Task 1), Rückgabe-Semantik-Tabelle abgedeckt durch Task 2 (unbrauchbar→false), Task 3 (Qualität 0→true/has_fix false).
- ddmm→dezimal + Vorzeichen ✅ (Task 4).
- Robustheit/abgeschnitten ✅ (Task 5).
- Doku-Update ✅ (Task 6).
- Alle 7 Spec-Tests abgebildet: bekannter Satz (Task 3+4), Süd/West (Task 4), Qualität 0 (Task 3), kaputte Checksumme (Task 2), Nicht-GGA (Task 2), abgeschnitten/leer (Task 1+5), Talker-Variante (Task 2).

**Placeholder scan:** Keine TBD/TODO; jeder Code-Schritt zeigt vollständigen Code.

**Type consistency:** `GpsFix`-Feldnamen (`has_fix, lat, lon, alt_gps_m, sats`) einheitlich in allen Tasks; `parse_gga(const std::string&, GpsFix&)` durchgehend; Hilfen `check_frame`, `split_fields`, `nmea_to_decimal`, `hex_val` konsistent benannt.

**Hinweis zu TinyGPSPlus:** `platformio.ini` listet `mikalhart/TinyGPSPlus` als Flight-Dependency. Diese Bibliothek ist Arduino-gebunden und für den nativen, hardware-freien Parser bewusst NICHT genutzt (Architektur-Regel: testbare Logik hardware-frei). Ob TinyGPSPlus im Flight-Env künftig noch gebraucht wird, ist eine spätere `src/flight`-Entscheidung — außerhalb dieses Plans.
