# BMP280-Umrechnung Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ein hardware-freies `lib/telemetry/bmp280`-Modul bauen, das BMP280-Rohwerte in Temperatur (°C), Druck (Pa) und barometrische Höhe (m) umrechnet, und diese Größen in den CSV-`TelemetryRecord` integrieren.

**Architecture:** Reine C++17-Arithmetik (Bosch-Integer-Kompensation, 32-bit-Variante), nativ getestet gegen verifizierte Referenzwerte. Kein I²C, kein `<Arduino.h>` — das Registerlesen bleibt später `src/flight`. CSV-Erweiterung nach der 3-Stellen-Regel aus `record.h` mit Round-Trip-Absicherung.

**Tech Stack:** C++17, PlatformIO `native`-Env, Unity-Testframework (Double-Asserts via `-D UNITY_INCLUDE_DOUBLE` aktiv).

**Spec:** `docs/superpowers/specs/2026-07-04-bmp280-umrechnung-design.md`

## Global Constraints

- Sprache im Projekt: **Deutsch** (Kommentare, Commits, Doku).
- **Hardware-frei:** `lib/telemetry/` darf kein `<Arduino.h>` einbinden. Alles läuft als nativer Unit-Test auf dem Mac.
- **TDD verbindlich:** erst der Test, ihn fehlschlagen sehen (RED aus dem richtigen Grund), dann minimaler Code (GREEN).
- **Referenz statt Annahme:** Byte-/Rechen-Logik gegen bekannt-korrekte Referenzwerte testen (Werte in den Tasks sind aus dem Bosch-Datenblatt-Code verifiziert, nicht geraten).
- **Namespace:** alles in `namespace telemetry`.
- **YAGNI:** nur 32-bit-Integer-Variante, nur BMP280. Keine double-/int64-Variante, kein Oversampling-Setup.
- **Test-Konvention:** ein Ordner pro Baustein unter `test/`, eigenes `main()` mit `UNITY_BEGIN/END` (sonst Linker-Kollision).
- **Kein Git-Repo:** Dieses Projekt ist (Stand jetzt) nicht unter Git-Versionierung. Die „Commit"-Schritte unten sind daher **optional** — falls `git rev-parse` fehlschlägt, überspringen und stattdessen den Task als abgeschlossen markieren.
- **Build/Test-Kommando:** `export PATH="$PATH:$HOME/.platformio/penv/bin"` einmalig, dann `pio test -e native`.

---

### Task 1: BMP280-Temperatur-Kompensation + Modulgerüst

Baut den Modul-Header `bmp280.h` (Calib-Struct + drei Funktionssignaturen) und implementiert die Temperatur-Kompensation. Enthält den Vorzeichen-Fallen-Test, weil die Temperatur-Konstanten (`dig_T3=-1000`) signed sind.

**Files:**
- Create: `lib/telemetry/bmp280.h`
- Create: `lib/telemetry/bmp280.cpp`
- Test: `test/test_bmp280/test_bmp280.cpp`

**Interfaces:**
- Consumes: nichts (neues Modul).
- Produces:
  - `struct telemetry::Bmp280Calib { uint16_t dig_T1; int16_t dig_T2, dig_T3; uint16_t dig_P1; int16_t dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9; };`
  - `int32_t telemetry::bmp280_compensate_temperature(int32_t adc_T, const Bmp280Calib& c, int32_t& t_fine);` — Rückgabe in 0,01 °C; setzt `t_fine`.
  - `uint32_t telemetry::bmp280_compensate_pressure(int32_t adc_P, const Bmp280Calib& c, int32_t t_fine);` — Rückgabe in Pa (implementiert in Task 2).
  - `float telemetry::bmp280_altitude_m(float pressure_pa, float sea_level_pa);` — Rückgabe in m (implementiert in Task 3).

- [ ] **Step 1: Header mit vollständiger API anlegen**

Create `lib/telemetry/bmp280.h`:

```cpp
// bmp280.h — BMP280-Umrechnung: Rohwerte + Kalibrierung -> physikalische Größen.
//
// Hardware-frei (kein <Arduino.h>) -> nativ testbar. Kennt NUR den BMP280,
// nicht das CSV-Format und KEIN I²C. Der src/flight-Aufrufer liest per I²C die
// Kalibrierkonstanten (einmalig) und die Rohwerte (pro Zyklus) und ruft diese
// Funktionen. Formeln exakt nach BST-BMP280-DS001 Rev 1.26, 3.11.3 (32-bit).

#ifndef TELEMETRY_BMP280_H
#define TELEMETRY_BMP280_H

#include <cstdint>

namespace telemetry {

// Kalibrierkonstanten (Register 0x88..0x9F). Typen exakt nach Datenblatt
// (3.11.2) — Vorzeichen entscheidend, sonst grob falsche Werte!
struct Bmp280Calib {
    uint16_t dig_T1;
    int16_t  dig_T2, dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2, dig_P3, dig_P4, dig_P5,
             dig_P6, dig_P7, dig_P8, dig_P9;
};

// adc_T = 20-bit-Rohwert (positiv, in 32-bit-signed). Rückgabe in 0,01 °C
// (2508 = 25,08 °C). Setzt t_fine (Zwischengröße für die Druckformel).
int32_t bmp280_compensate_temperature(int32_t adc_T, const Bmp280Calib& c,
                                       int32_t& t_fine);

// adc_P = 20-bit-Rohwert. t_fine MUSS vorher von compensate_temperature stammen.
// Rückgabe in Pa direkt (100656 = 1006,56 hPa).
uint32_t bmp280_compensate_pressure(int32_t adc_P, const Bmp280Calib& c,
                                     int32_t t_fine);

// Barometrische Höhe (m) aus Druck (Pa) + Referenzdruck QNH (Pa).
float bmp280_altitude_m(float pressure_pa, float sea_level_pa);

} // namespace telemetry

#endif // TELEMETRY_BMP280_H
```

- [ ] **Step 2: Failing test für Temperatur + Vorzeichen-Falle schreiben**

Create `test/test_bmp280/test_bmp280.cpp`:

```cpp
// test_bmp280.cpp — native Tests für die BMP280-Umrechnung.
//   pio test -e native
//
// Referenzwerte verifiziert aus dem Datenblatt-C-Code (BST-BMP280-DS001
// Rev 1.26, 3.11.3), NICHT aus dem Gedächtnis. Kalibriersatz + Ergebnisse:
//   adc_T=519888 -> t_fine=128422, T=2508 (25,08 °C)
//   adc_P=415148 -> 100656 Pa (1006,56 hPa)

#include <unity.h>
#include "bmp280.h"

using namespace telemetry;

void setUp() {}
void tearDown() {}

// Klassischer, in mehreren Bibliotheken zitierter Kalibriersatz.
static Bmp280Calib ref_calib() {
    Bmp280Calib c;
    c.dig_T1 = 27504;  c.dig_T2 = 26435;  c.dig_T3 = -1000;
    c.dig_P1 = 36477;  c.dig_P2 = -10685; c.dig_P3 = 3024;
    c.dig_P4 = 2855;   c.dig_P5 = 140;    c.dig_P6 = -7;
    c.dig_P7 = 15500;  c.dig_P8 = -14600; c.dig_P9 = 6000;
    return c;
}

// Temperatur gegen den verifizierten Referenzwert (25,08 °C) + t_fine.
void test_temperature_reference() {
    Bmp280Calib c = ref_calib();
    int32_t t_fine = 0;
    int32_t T = bmp280_compensate_temperature(519888, c, t_fine);
    TEST_ASSERT_EQUAL_INT32(128422, t_fine);
    TEST_ASSERT_EQUAL_INT32(2508, T);   // 25,08 °C
}

// Vorzeichen-Falle: negative Kalibrierkonstanten müssen als signed wirken.
// Würde dig_T3=-1000 fälschlich als uint16 (65536-1000) gelesen, käme ein
// grob anderer t_fine heraus -> dieser Test schlägt dann fehl.
void test_temperature_signed_calib() {
    Bmp280Calib c = ref_calib();
    int32_t t_fine = 0;
    bmp280_compensate_temperature(519888, c, t_fine);
    TEST_ASSERT_EQUAL_INT32(128422, t_fine);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_temperature_reference);
    RUN_TEST(test_temperature_signed_calib);
    return UNITY_END();
}
```

- [ ] **Step 3: Test laufen lassen und Fehlschlag verifizieren**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && pio test -e native -f test_bmp280`
Expected: FAIL — Linker-Fehler „undefined reference to `bmp280_compensate_temperature`" (Funktion noch nicht implementiert).

- [ ] **Step 4: Temperatur-Kompensation implementieren**

Create `lib/telemetry/bmp280.cpp`:

```cpp
// bmp280.cpp — BMP280-Umrechnung (siehe bmp280.h). Hardware-frei.
// Integer-Kompensation exakt nach BST-BMP280-DS001 Rev 1.26, 3.11.3.

#include "bmp280.h"

namespace telemetry {

int32_t bmp280_compensate_temperature(int32_t adc_T, const Bmp280Calib& c,
                                       int32_t& t_fine) {
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)c.dig_T1 << 1))) *
                    ((int32_t)c.dig_T2)) >> 11;
    int32_t var2 = (((((adc_T >> 4) - ((int32_t)c.dig_T1)) *
                      ((adc_T >> 4) - ((int32_t)c.dig_T1))) >> 12) *
                    ((int32_t)c.dig_T3)) >> 14;
    t_fine = var1 + var2;
    return (t_fine * 5 + 128) >> 8;
}

} // namespace telemetry
```

- [ ] **Step 5: Test laufen lassen und Erfolg verifizieren**

Run: `pio test -e native -f test_bmp280`
Expected: PASS — 2 Tests grün (`test_temperature_reference`, `test_temperature_signed_calib`).

- [ ] **Step 6: Commit (optional, nur falls Git-Repo)**

```bash
git rev-parse --is-inside-work-tree >/dev/null 2>&1 && \
  git add lib/telemetry/bmp280.h lib/telemetry/bmp280.cpp test/test_bmp280/test_bmp280.cpp && \
  git commit -m "feat(bmp280): Temperatur-Kompensation (Bosch int32) + Modulgerüst"
```

---

### Task 2: BMP280-Druck-Kompensation

Implementiert die Druckformel, die `t_fine` aus Task 1 nutzt und Pa direkt zurückgibt.

**Files:**
- Modify: `lib/telemetry/bmp280.cpp` (Funktion `bmp280_compensate_pressure` ergänzen)
- Modify: `test/test_bmp280/test_bmp280.cpp` (Test + `RUN_TEST` ergänzen)

**Interfaces:**
- Consumes: `bmp280_compensate_temperature(...)` (für `t_fine`), `Bmp280Calib` — beide aus Task 1.
- Produces: `uint32_t bmp280_compensate_pressure(int32_t adc_P, const Bmp280Calib& c, int32_t t_fine);` — Rückgabe in Pa.

- [ ] **Step 1: Failing test für Druck schreiben**

Modify `test/test_bmp280/test_bmp280.cpp` — Test-Funktion **vor** `main()` einfügen:

```cpp
// Druck gegen den verifizierten Referenzwert (100656 Pa). Nutzt t_fine aus der
// Temperatur-Kompensation — Reihenfolge wie im echten loop().
void test_pressure_reference() {
    Bmp280Calib c = ref_calib();
    int32_t t_fine = 0;
    bmp280_compensate_temperature(519888, c, t_fine);   // füllt t_fine
    uint32_t p = bmp280_compensate_pressure(415148, c, t_fine);
    TEST_ASSERT_EQUAL_UINT32(100656, p);   // 1006,56 hPa
}
```

In `main()` die Zeile ergänzen (nach den bestehenden `RUN_TEST`):

```cpp
    RUN_TEST(test_pressure_reference);
```

- [ ] **Step 2: Test laufen lassen und Fehlschlag verifizieren**

Run: `pio test -e native -f test_bmp280`
Expected: FAIL — Linker-Fehler „undefined reference to `bmp280_compensate_pressure`".

- [ ] **Step 3: Druck-Kompensation implementieren**

Modify `lib/telemetry/bmp280.cpp` — Funktion **vor** dem schließenden `} // namespace telemetry` einfügen:

```cpp
uint32_t bmp280_compensate_pressure(int32_t adc_P, const Bmp280Calib& c,
                                     int32_t t_fine) {
    int32_t var1 = (((int32_t)t_fine) >> 1) - (int32_t)64000;
    int32_t var2 = (((var1 >> 2) * (var1 >> 2)) >> 11) * ((int32_t)c.dig_P6);
    var2 = var2 + ((var1 * ((int32_t)c.dig_P5)) << 1);
    var2 = (var2 >> 2) + (((int32_t)c.dig_P4) << 16);
    var1 = (((c.dig_P3 * (((var1 >> 2) * (var1 >> 2)) >> 13)) >> 3) +
            ((((int32_t)c.dig_P2) * var1) >> 1)) >> 18;
    var1 = ((((32768 + var1)) * ((int32_t)c.dig_P1)) >> 15);
    if (var1 == 0) {
        return 0;  // Division durch Null vermeiden
    }
    uint32_t p = (((uint32_t)(((int32_t)1048576) - adc_P) - (var2 >> 12))) * 3125;
    if (p < 0x80000000) {
        p = (p << 1) / ((uint32_t)var1);
    } else {
        p = (p / (uint32_t)var1) * 2;
    }
    var1 = (((int32_t)c.dig_P9) * ((int32_t)(((p >> 3) * (p >> 3)) >> 13))) >> 12;
    var2 = (((int32_t)(p >> 2)) * ((int32_t)c.dig_P8)) >> 13;
    p = (uint32_t)((int32_t)p + ((var1 + var2 + c.dig_P7) >> 4));
    return p;
}
```

- [ ] **Step 4: Test laufen lassen und Erfolg verifizieren**

Run: `pio test -e native -f test_bmp280`
Expected: PASS — 3 Tests grün.

- [ ] **Step 5: Commit (optional, nur falls Git-Repo)**

```bash
git rev-parse --is-inside-work-tree >/dev/null 2>&1 && \
  git add lib/telemetry/bmp280.cpp test/test_bmp280/test_bmp280.cpp && \
  git commit -m "feat(bmp280): Druck-Kompensation (Bosch int32, Pa direkt)"
```

---

### Task 3: Barometrische Höhe

Implementiert die internationale Höhenformel mit QNH als Parameter.

**Files:**
- Modify: `lib/telemetry/bmp280.cpp` (Funktion `bmp280_altitude_m` ergänzen, `<cmath>` einbinden)
- Modify: `test/test_bmp280/test_bmp280.cpp` (Tests + `RUN_TEST` ergänzen)

**Interfaces:**
- Consumes: nichts Neues.
- Produces: `float bmp280_altitude_m(float pressure_pa, float sea_level_pa);` — Rückgabe in m.

- [ ] **Step 1: Failing tests für Höhe schreiben**

Modify `test/test_bmp280/test_bmp280.cpp` — vor `main()` einfügen:

```cpp
// Höhe: gleicher Druck wie Referenzdruck -> 0 m.
void test_altitude_zero_at_sea_level() {
    float h = bmp280_altitude_m(101325.0f, 101325.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, h);
}

// Höhe: verifizierter Standardpunkt (p=89876 Pa, QNH=101325 Pa) -> ~1000 m.
void test_altitude_reference_point() {
    float h = bmp280_altitude_m(89876.0f, 101325.0f);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 1000.0f, h);   // 1000,02 m ± 1 m
}
```

In `main()` ergänzen:

```cpp
    RUN_TEST(test_altitude_zero_at_sea_level);
    RUN_TEST(test_altitude_reference_point);
```

- [ ] **Step 2: Test laufen lassen und Fehlschlag verifizieren**

Run: `pio test -e native -f test_bmp280`
Expected: FAIL — Linker-Fehler „undefined reference to `bmp280_altitude_m`".

- [ ] **Step 3: Höhenformel implementieren**

Modify `lib/telemetry/bmp280.cpp`:

Oben, direkt nach `#include "bmp280.h"`, ergänzen:

```cpp
#include <cmath>
```

Vor dem schließenden `} // namespace telemetry` einfügen:

```cpp
float bmp280_altitude_m(float pressure_pa, float sea_level_pa) {
    // Internationale barometrische Höhenformel (Standardatmosphäre).
    return 44330.0f * (1.0f - std::pow(pressure_pa / sea_level_pa,
                                       1.0f / 5.255f));
}
```

- [ ] **Step 4: Test laufen lassen und Erfolg verifizieren**

Run: `pio test -e native -f test_bmp280`
Expected: PASS — 5 Tests grün.

- [ ] **Step 5: Commit (optional, nur falls Git-Repo)**

```bash
git rev-parse --is-inside-work-tree >/dev/null 2>&1 && \
  git add lib/telemetry/bmp280.cpp test/test_bmp280/test_bmp280.cpp && \
  git commit -m "feat(bmp280): barometrische Höhe (QNH als Parameter)"
```

---

### Task 4: CSV-Integration (3-Stellen-Erweiterung)

Nimmt `has_bmp` + `temp_c`, `pressure_hpa`, `alt_baro_m` in den `TelemetryRecord` auf und erweitert `csv_header`, `csv_row`, `parse_csv_row`. Round-Trip-Absicherung.

**Files:**
- Modify: `lib/telemetry/record.h` (Felder ergänzen)
- Modify: `lib/telemetry/record.cpp` (`csv_header`, `csv_row`, `parse_csv_row`)
- Modify: `test/test_record/test_record.cpp` (Tests + `RUN_TEST`, bestehenden Test anpassen)

**Interfaces:**
- Consumes: nichts aus Task 1–3 (Record speichert fertige °C/hPa/m-Werte; die Umrechnung macht später `src/flight`).
- Produces: erweitertes CSV-Format mit Spalten `...,sats,temp_c,pressure_hpa,alt_baro_m` (10 Spalten statt 7).

- [ ] **Step 1: Failing tests schreiben + bestehenden Spaltenzahl-Test anpassen**

Modify `test/test_record/test_record.cpp`:

**(a)** Den bestehenden Test `test_wrong_column_count_rejected` anpassen — die neue Spaltenzahl ist 11, eine 7-Spalten-Zeile muss jetzt abgelehnt werden:

```cpp
// Falsche Spaltenzahl (7 statt 10) -> abgelehnt.
void test_wrong_column_count_rejected() {
    TelemetryRecord out;
    // alte 7-Spalten-Zeile ohne BMP-Felder
    TEST_ASSERT_FALSE(parse_csv_row("123,,PREFLIGHT,,,,", out));
}
```

**(b)** Neue Tests vor `main()` einfügen:

```cpp
// Header endet mit den drei BMP280-Spalten in fester Reihenfolge.
void test_header_ends_with_bmp_columns() {
    std::string h = csv_header();
    std::string suffix = "temp_c,pressure_hpa,alt_baro_m";
    TEST_ASSERT_TRUE(h.size() >= suffix.size());
    TEST_ASSERT_EQUAL_STRING(suffix.c_str(),
                             h.substr(h.size() - suffix.size()).c_str());
}

// Ohne BMP-Sensor bleiben die drei Spalten leer (kein 0.0 vortäuschen).
void test_row_without_bmp_has_empty_fields() {
    TelemetryRecord r;
    r.t_ms = 1;
    r.has_bmp = false;
    std::string row = csv_row(r);
    // Zeile endet mit drei leeren Feldern: ",,"  (temp, pressure, alt)
    TEST_ASSERT_EQUAL_STRING(",,",
                             row.substr(row.size() - 2).c_str());
}

// Round-Trip MIT BMP-Werten.
void test_roundtrip_with_bmp() {
    TelemetryRecord in;
    in.t_ms = 3000;
    in.phase = Phase::Ascent;
    in.has_bmp = true;
    in.temp_c = 25.08f;
    in.pressure_hpa = 1006.56f;
    in.alt_baro_m = 55.85f;

    TelemetryRecord out;
    TEST_ASSERT_TRUE(parse_csv_row(csv_row(in), out));
    TEST_ASSERT_TRUE(out.has_bmp);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, in.temp_c, out.temp_c);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, in.pressure_hpa, out.pressure_hpa);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, in.alt_baro_m, out.alt_baro_m);
}

// Round-Trip OHNE BMP -> has_bmp false zurück.
void test_roundtrip_without_bmp() {
    TelemetryRecord in;
    in.t_ms = 4000;
    in.has_bmp = false;

    TelemetryRecord out;
    out.has_bmp = true;  // bewusst vorbelegen, muss überschrieben werden
    TEST_ASSERT_TRUE(parse_csv_row(csv_row(in), out));
    TEST_ASSERT_FALSE(out.has_bmp);
}
```

In `main()` ergänzen (nach den bestehenden `RUN_TEST`):

```cpp
    RUN_TEST(test_header_ends_with_bmp_columns);
    RUN_TEST(test_row_without_bmp_has_empty_fields);
    RUN_TEST(test_roundtrip_with_bmp);
    RUN_TEST(test_roundtrip_without_bmp);
```

- [ ] **Step 2: Test laufen lassen und Fehlschlag verifizieren**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && pio test -e native -f test_record`
Expected: FAIL — Kompilierfehler (`has_bmp`/`temp_c`/`pressure_hpa`/`alt_baro_m` sind keine Member von `TelemetryRecord`).

- [ ] **Step 3: Record-Felder ergänzen (Stelle 1)**

Modify `lib/telemetry/record.h` — die Kommentarzeile für weitere Sensoren ersetzen durch:

```cpp
    // --- BMP280 (Temperatur/Druck/Barometer-Höhe), ein gemeinsames Flag ---
    bool  has_bmp = false;
    float temp_c = 0.0f;        // °C
    float pressure_hpa = 0.0f;  // hPa
    float alt_baro_m = 0.0f;    // barometrische Höhe (m), aus Druck + QNH
```

- [ ] **Step 4: `csv_header` erweitern (Stelle 2)**

Modify `lib/telemetry/record.cpp` — `csv_header()`:

```cpp
std::string csv_header() {
    return "t_ms,utc,phase,lat,lon,alt_gps_m,sats,temp_c,pressure_hpa,alt_baro_m";
}
```

- [ ] **Step 5: `csv_row` erweitern (Stelle 3a)**

Modify `lib/telemetry/record.cpp` — in `csv_row()` **vor** `return s;` einfügen:

```cpp
    s += ',';
    // BMP280: nur bei bestücktem Sensor Werte, sonst drei leere Felder.
    if (r.has_bmp) {
        s += num(r.temp_c, 2);       s += ',';
        s += num(r.pressure_hpa, 2); s += ',';
        s += num(r.alt_baro_m, 2);
    } else {
        s += ",,";  // temp_c, pressure_hpa, alt_baro_m alle leer
    }
```

- [ ] **Step 6: `parse_csv_row` erweitern (Stelle 3b)**

Modify `lib/telemetry/record.cpp` — in `parse_csv_row()`:

Die Spaltenzahl-Prüfung von 7 auf 11 ändern:

```cpp
    if (f.size() != 10) return false;          // Spaltenzahl muss passen
```

Vor `return true;` einfügen:

```cpp
    // BMP280 (Felder [7]/[8]/[9]): gilt als vorhanden, wenn temp_c befüllt ist.
    if (!f[7].empty()) {
        out.has_bmp = true;
        out.temp_c = static_cast<float>(std::strtod(f[7].c_str(), nullptr));
        if (!f[8].empty()) out.pressure_hpa = static_cast<float>(std::strtod(f[8].c_str(), nullptr));
        if (!f[9].empty()) out.alt_baro_m   = static_cast<float>(std::strtod(f[9].c_str(), nullptr));
    }
```

- [ ] **Step 7: Test laufen lassen und Erfolg verifizieren**

Run: `pio test -e native -f test_record`
Expected: PASS — alle Record-Tests grün (13 alte, davon 1 angepasst + 4 neue).

- [ ] **Step 8: Gesamte native Suite laufen lassen**

Run: `pio test -e native`
Expected: PASS — alle Test-Ordner grün (bestehende 20 Tests + bmp280 + neue record-Tests).

- [ ] **Step 9: Commit (optional, nur falls Git-Repo)**

```bash
git rev-parse --is-inside-work-tree >/dev/null 2>&1 && \
  git add lib/telemetry/record.h lib/telemetry/record.cpp test/test_record/test_record.cpp && \
  git commit -m "feat(record): BMP280-Spalten (temp_c, pressure_hpa, alt_baro_m) ins CSV"
```

---

### Task 5: Software-Flow-Doku aktualisieren

CLAUDE.md verlangt, `docs/software-flow.md` nach jeder Ablaufänderung mitzupflegen.

**Files:**
- Modify: `docs/software-flow.md`

**Interfaces:**
- Consumes: den fertigen `bmp280`-Baustein (Task 1–3) und die CSV-Erweiterung (Task 4).
- Produces: keine Code-Schnittstelle.

- [ ] **Step 1: Diagramm §1 (Architektur) ergänzen**

In `docs/software-flow.md`, im `LIB`-Subgraph des ersten Mermaid-Diagramms, einen Knoten neben `REC` ergänzen:

```
        BMP["bmp280.cpp<br/>BMP280-Umrechnung (Temp/Druck/Höhe)"]
```

und am Ende der `style`-Zeilen:

```
    style BMP fill:#d4edda
```

- [ ] **Step 2: `loop()`-Diagramm §3 aktualisieren**

Den Knoten `READ_SENS` von „⬜ BMP280, MPU-6050, DS18B20, UV" ändern zu:

```
    READ_SENS["Sensoren lesen<br/>🔶 BMP280 (Umrechnung ✅), ⬜ MPU-6050, DS18B20, UV"]
```

und die Style-Zeile für `READ_SENS` von `fill:#f8f9fa` auf `fill:#fff3cd` setzen (teilweise begonnen).

- [ ] **Step 3: Landkarten-Tabelle §6 ergänzen**

In der Tabelle die Sensor-Zeile ersetzen/ergänzen:

```
| BMP280-Umrechnung (Temp/Druck/Höhe) | `lib/telemetry/bmp280` | ✅ nativ getestet |
| BMP280 CSV-Spalten | `lib/telemetry/record` | ✅ nativ getestet |
| Sensor-Lesung (I²C BMP280, MPU, DS18B20, UV) | `src/flight/` | ⬜ am Board |
```

- [ ] **Step 4: Abschließenden Fließtext in §6 ergänzen**

Am Ende von §6 einen Absatz ergänzen, der festhält: die **BMP280-Umrechnung** ist nun hardware-frei umgesetzt und nativ getestet (Temperatur/Druck gegen verifizierte Datenblatt-Referenzwerte, Höhe gegen die barometrische Formel); die drei Spalten `temp_c, pressure_hpa, alt_baro_m` sind im CSV-Record integriert (Round-Trip grün). Offen bleibt das **I²C-Lesen** der Roh-/Kalibrierwerte in `src/flight` (am Board zu verifizieren) sowie die übrigen Sensoren.

- [ ] **Step 5: Commit (optional, nur falls Git-Repo)**

```bash
git rev-parse --is-inside-work-tree >/dev/null 2>&1 && \
  git add docs/software-flow.md && \
  git commit -m "docs(flow): BMP280-Umrechnung + CSV-Spalten als fertig markieren"
```

---

## Self-Review

**1. Spec coverage:**
- Modul `bmp280.h/.cpp`, drei Funktionen, `t_fine` als Parameter → Task 1–3. ✅
- Integer-32-bit-Variante, Pa direkt → Task 2. ✅
- Verifizierte Referenzwerte (Temp 2508/t_fine 128422, Druck 100656, Höhe 0/1000) → Tasks 1–3. ✅
- Vorzeichen-Falle → Task 1 Step 2. ✅
- CSV 3-Stellen-Erweiterung mit `has_bmp` + 3 Feldern, Round-Trip → Task 4. ✅
- Grenzen (kein I²C in lib, nur BMP280) → in Global Constraints + Interfaces festgehalten. ✅
- Software-Flow mitpflegen → Task 5. ✅

**2. Placeholder scan:** Keine TBD/„handle edge cases"/„ähnlich wie". Aller Code ausgeschrieben, alle Zahlen konkret.

**3. Type consistency:** `bmp280_compensate_temperature` / `_pressure` / `bmp280_altitude_m`, `Bmp280Calib` durchgängig identisch in Header, Impl und Tests. Feldnamen `has_bmp/temp_c/pressure_hpa/alt_baro_m` konsistent zwischen record.h, record.cpp und Test. Spaltenzahl **10** konsistent: `csv_header()` listet 10 Spalten (9 Kommata), `parse_csv_row` prüft `f.size() != 10`, Felder [7]/[8]/[9] sind die BMP-Spalten.
