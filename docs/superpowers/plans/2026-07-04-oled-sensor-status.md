# OLED-Sensor-Status Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Das board-interne OLED zeigt vor dem Start als „Startklar?"-Check, welche Komponenten (GPS, microSD) funktionieren, und schaltet beim Flugstart ab.

**Architecture:** Reine Textformatierung der Statuszeilen liegt hardware-frei in `lib/telemetry/display_status` (nativ per TDD getestet). Das Zeichnen (U8g2) und das Vext-Schalten liegt in `src/flight/oled` (am Board verifiziert). `main.cpp` verdrahtet beides und schaltet beim Phasenübergang `PreFlight → Ascent` ab.

**Tech Stack:** C++17, PlatformIO, Unity (native Tests), U8g2 (SSD1306, nur `flight`-Env).

## Global Constraints

- Sprache: Deutsch (Doku, Kommentare, Commits).
- TDD: Erst Test, fehlschlagen sehen, dann minimaler Code.
- YAGNI: Nur GPS + microSD anzeigen. KEINE Platzhalter für BMP280/MPU/DS18B20/UV.
- Logik/Hardware-Trennung: Testbares nach `lib/telemetry/`, Hardware nach `src/flight/`.
- Board V2: OLED-Pins clock=15, data=4, reset=16. Vext = GPIO21 (LOW=an, HIGH=aus).
- Kein Git-Repo → Commit-Steps entfallen (stattdessen: Zwischenstand notieren). Falls das Repo später initialisiert wird, Commits nachholen.
- Native Tests: pro Baustein eigener Ordner mit eigenem `main()` (UNITY_BEGIN/END).

---

### Task 1: Statuszeilen-Logik (`display_status`, hardware-frei)

**Files:**
- Create: `lib/telemetry/display_status.h`
- Create: `lib/telemetry/display_status.cpp`
- Test: `test/test_display_status/test_display_status.cpp`

**Interfaces:**
- Consumes: `telemetry::Phase` und `telemetry::to_string(Phase)` aus `lib/telemetry/flight_phase.h`.
- Produces:
  - `enum class telemetry::GpsDisp { Silent, Waiting, Fix };`
  - `struct telemetry::DisplayState { GpsDisp gps; uint8_t sats; bool sd_ok; Phase phase; };`
  - `std::vector<std::string> telemetry::status_lines(const DisplayState&);`
  - Zeilen-Reihenfolge (Layout-Kontrakt): `[0]="Wetterballon"`, `[1]="GPS: ..."`, `[2]="SD: ..."`, `[3]="Phase: ..."`.

- [ ] **Step 1: Failing test schreiben**

`test/test_display_status/test_display_status.cpp`:
```cpp
// test_display_status.cpp — native Tests für die OLED-Statuszeilen.
//   pio test -e native
// Reine Textformatierung (hardware-frei). Prüft Layout-Kontrakt + GPS-Stufen.
#include <unity.h>
#include "display_status.h"
#include <string>
#include <vector>

using namespace telemetry;

void setUp() {}
void tearDown() {}

static DisplayState base() {
    return DisplayState{ GpsDisp::Silent, 0, false, Phase::PreFlight };
}

void test_layout_has_four_lines_in_order() {
    auto l = status_lines(base());
    TEST_ASSERT_EQUAL_INT(4, (int)l.size());
    TEST_ASSERT_EQUAL_STRING("Wetterballon", l[0].c_str());
    TEST_ASSERT_TRUE(l[1].rfind("GPS:", 0) == 0);
    TEST_ASSERT_TRUE(l[2].rfind("SD:", 0) == 0);
    TEST_ASSERT_TRUE(l[3].rfind("Phase:", 0) == 0);
}

void test_gps_silent() {
    auto s = base(); s.gps = GpsDisp::Silent;
    TEST_ASSERT_EQUAL_STRING("GPS: --", status_lines(s)[1].c_str());
}

void test_gps_waiting() {
    auto s = base(); s.gps = GpsDisp::Waiting;
    TEST_ASSERT_EQUAL_STRING("GPS: warte...", status_lines(s)[1].c_str());
}

void test_gps_fix_shows_sat_count() {
    auto s = base(); s.gps = GpsDisp::Fix; s.sats = 7;
    TEST_ASSERT_EQUAL_STRING("GPS: Fix 7 Sat", status_lines(s)[1].c_str());
}

void test_sd_ok_and_missing() {
    auto s = base(); s.sd_ok = true;
    TEST_ASSERT_EQUAL_STRING("SD: ok", status_lines(s)[2].c_str());
    s.sd_ok = false;
    TEST_ASSERT_EQUAL_STRING("SD: --", status_lines(s)[2].c_str());
}

void test_phase_uses_to_string() {
    auto s = base(); s.phase = Phase::Ascent;
    std::string expected = std::string("Phase: ") + to_string(Phase::Ascent);
    TEST_ASSERT_EQUAL_STRING(expected.c_str(), status_lines(s)[3].c_str());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_layout_has_four_lines_in_order);
    RUN_TEST(test_gps_silent);
    RUN_TEST(test_gps_waiting);
    RUN_TEST(test_gps_fix_shows_sat_count);
    RUN_TEST(test_sd_ok_and_missing);
    RUN_TEST(test_phase_uses_to_string);
    return UNITY_END();
}
```

- [ ] **Step 2: Test laufen lassen, Fehlschlag sehen**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && pio test -e native -f test_display_status`
Expected: FAIL — `display_status.h: No such file or directory` (Header existiert noch nicht).

- [ ] **Step 3: Header schreiben**

`lib/telemetry/display_status.h`:
```cpp
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

struct DisplayState {
    GpsDisp gps;
    uint8_t sats;
    bool    sd_ok;
    Phase   phase;
};

// Anzuzeigende Zeilen. Reihenfolge = Layout-Kontrakt:
//   [0] Titel, [1] GPS, [2] SD, [3] Phase.
std::vector<std::string> status_lines(const DisplayState& s);

} // namespace telemetry

#endif // TELEMETRY_DISPLAY_STATUS_H
```

- [ ] **Step 4: Minimale Implementierung schreiben**

`lib/telemetry/display_status.cpp`:
```cpp
// display_status.cpp — siehe Header.
#include "display_status.h"

namespace telemetry {

std::vector<std::string> status_lines(const DisplayState& s) {
    std::string gps;
    switch (s.gps) {
        case GpsDisp::Silent:  gps = "GPS: --";       break;
        case GpsDisp::Waiting: gps = "GPS: warte...";  break;
        case GpsDisp::Fix:     gps = "GPS: Fix " + std::to_string((int)s.sats) + " Sat"; break;
    }
    return {
        "Wetterballon",
        gps,
        s.sd_ok ? "SD: ok" : "SD: --",
        std::string("Phase: ") + to_string(s.phase),
    };
}

} // namespace telemetry
```

- [ ] **Step 5: Tests laufen lassen, grün sehen**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && pio test -e native -f test_display_status`
Expected: PASS (6 Tests). Danach zur Sicherheit alle: `pio test -e native` → weiterhin grün.

- [ ] **Step 6: Zwischenstand notieren** (kein Git-Repo)

Notiz: „Task 1 fertig — display_status Logik + 6 native Tests grün."

---

### Task 2: OLED-Hardware-Modul (`src/flight/oled`)

**Files:**
- Create: `src/flight/oled.h`
- Create: `src/flight/oled.cpp`
- Modify: `platformio.ini` (U8g2 als lib_deps im `esp32_base` bzw. `flight`)

**Interfaces:**
- Consumes: `PIN_VEXT`, `PIN_I2C_SDA`, `PIN_I2C_SCL` aus `src/flight/pins.h`; `std::vector<std::string>` aus `display_status`.
- Produces:
  - `void oled_begin();` — Vext an, U8g2 init.
  - `void oled_show(const std::vector<std::string>& lines);` — Zeilen zeichnen.
  - `void oled_off();` — Display Power-Save + Vext aus.

Hinweis: Dieses Modul ist Hardware-Code und wird NICHT nativ getestet, sondern am Board verifiziert (siehe TODO-Erweiterung). Kein TDD-Zyklus, dafür Kompilier-Gate.

- [ ] **Step 1: U8g2 als Dependency ergänzen**

In `platformio.ini` unter `[esp32_base]` `lib_deps` diese Zeile ergänzen:
```ini
    olikraus/U8g2                       ; SSD1306 OLED (Heltec V2, board-intern)
```

- [ ] **Step 2: Header schreiben**

`src/flight/oled.h`:
```cpp
// oled.h — board-internes SSD1306-OLED (Heltec V2). Hardware-Teil.
//
// Zeigt den „Startklar?"-Check am Boden. Formatierung der Zeilen kommt aus
// lib/telemetry/display_status (nativ getestet); hier nur Init/Zeichnen/Abschalten.
// Pins: clock=SCL(15), data=SDA(4), reset=16. Versorgung via Vext (GPIO21, LOW=an).
#ifndef FLIGHT_OLED_H
#define FLIGHT_OLED_H

#include <string>
#include <vector>

// Vext an (GPIO21 LOW), kurz warten, U8g2 initialisieren.
void oled_begin();

// Zeilen zeichnen (eine pro Textzeile, oben beginnend).
void oled_show(const std::vector<std::string>& lines);

// Display in Power-Save + Vext aus (GPIO21 HIGH). Für Flugstart.
void oled_off();

#endif // FLIGHT_OLED_H
```

- [ ] **Step 3: Implementierung schreiben**

`src/flight/oled.cpp`:
```cpp
// oled.cpp — siehe Header. U8g2, SSD1306 128x64, HW-I2C.
#include "oled.h"
#include "pins.h"
#include <Arduino.h>
#include <U8g2lib.h>

// V2-Pinbelegung (verifiziert): reset=16, clock=SCL(15), data=SDA(4).
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C
    g_u8g2(U8G2_R0, /*reset=*/16, /*clock=*/PIN_I2C_SCL, /*data=*/PIN_I2C_SDA);

void oled_begin() {
    pinMode(PIN_VEXT, OUTPUT);
    digitalWrite(PIN_VEXT, LOW);   // Vext AN (versorgt OLED/Sensoren)
    delay(50);                     // Rail + Display kurz stabilisieren
    g_u8g2.begin();
    g_u8g2.setFont(u8g2_font_6x12_tf);
}

void oled_show(const std::vector<std::string>& lines) {
    g_u8g2.clearBuffer();
    int y = 12;                    // erste Grundlinie (Fonthöhe ~12 px)
    for (const auto& line : lines) {
        g_u8g2.drawStr(0, y, line.c_str());
        y += 13;                   // Zeilenabstand
    }
    g_u8g2.sendBuffer();
}

void oled_off() {
    g_u8g2.setPowerSave(1);        // Display-Controller schlafen legen
    digitalWrite(PIN_VEXT, HIGH);  // Vext AUS (Strom sparen im Flug)
}
```

- [ ] **Step 4: Kompilieren (Gate statt Test)**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && pio run -e flight`
Expected: Build erfolgreich (U8g2 wird heruntergeladen). Bei Fehler: Konstruktor-Signatur/Include prüfen.

- [ ] **Step 5: Zwischenstand notieren**

Notiz: „Task 2 fertig — oled-Modul kompiliert im flight-Env. Board-Verifikation offen (TODO)."

---

### Task 3: Integration in `main.cpp` + Software-Flow

**Files:**
- Modify: `src/flight/main.cpp`
- Modify: `docs/software-flow.md`
- Modify: `TODO.md` (Board-Test-Punkt ergänzen)

**Interfaces:**
- Consumes: `oled_begin/oled_show/oled_off` (Task 2), `status_lines`/`DisplayState`/`GpsDisp` (Task 1), vorhandenes `g_rec`, `g_detector`, `sd_log_begin`-Rückgabe.
- Produces: keine (Endverbraucher).

- [ ] **Step 1: Includes + Zustandsvariablen ergänzen**

In `src/flight/main.cpp` nach den bestehenden Includes ergänzen:
```cpp
#include "oled.h"
#include "display_status.h"
```
Bei den `static`-Globals ergänzen:
```cpp
static bool g_sd_ok       = false;  // Ergebnis von sd_log_begin(), fürs Display
static bool g_gps_seen    = false;  // schon je eine GGA geparst? (GPS-Stufe)
static bool g_oled_active = true;   // Display läuft, bis PreFlight verlassen wird
```

- [ ] **Step 2: `setup()` anpassen**

`sd_log_begin();` ersetzen durch:
```cpp
    g_sd_ok = sd_log_begin();
```
Am Ende von `setup()` (nach der Header-Ausgabe) ergänzen:
```cpp
    // OLED starten (Vext an, Init) — zeigt den Boden-Check.
    oled_begin();
```

- [ ] **Step 3: Display-Update in `loop()` einhängen**

In `loop()`, nach `if (fix.has_fix)`-Block und vor `sd_log(csv)`, `g_gps_seen` setzen: direkt nach `if (!parse_gga(line, fix)) continue;` steht fest, dass eine GGA geparst wurde — dort ergänzen:
```cpp
        g_gps_seen = true;
```
Nach dem Schreiben von `csv` (nach `sd_log(csv);`) den Display-Block ergänzen:
```cpp
        // Boden-Check anzeigen, solange noch nicht gestartet.
        if (g_oled_active) {
            if (g_rec.phase == Phase::PreFlight) {
                DisplayState ds;
                ds.gps   = fix.has_fix ? GpsDisp::Fix
                         : g_gps_seen  ? GpsDisp::Waiting
                                       : GpsDisp::Silent;
                ds.sats  = fix.sats;
                ds.sd_ok = g_sd_ok;
                ds.phase = g_rec.phase;
                oled_show(status_lines(ds));
            } else {
                oled_off();          // Flug begonnen -> Display aus (einmalig)
                g_oled_active = false;
            }
        }
```

- [ ] **Step 4: Kompilieren**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && pio run -e flight`
Expected: Build erfolgreich.

- [ ] **Step 5: Software-Flow + TODO aktualisieren**

In `docs/software-flow.md` den OLED-Baustein ergänzen: `oled_begin()` in setup(), Display-Update in loop() während `PreFlight`, `oled_off()` beim Übergang zu `Ascent`. Statusmarker: Logik ✅ (nativ getestet), Board-Verifikation 🔶.

In `TODO.md` unter „Testreihenfolge" ergänzen:
```markdown
  - [ ] OLED-Boden-Check am Board: Zeilen plausibel; beim Übergang PreFlight→Ascent
        geht das Display aus (Vext-Abschaltung verifizieren — OLED↔Vext-Kopplung V2)
```

- [ ] **Step 6: Alle nativen Tests grün + Zwischenstand**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && pio test -e native`
Expected: alle Tests PASS (bestehende + 6 neue).
Notiz: „Task 3 fertig — OLED in main.cpp integriert, flight kompiliert, Doku aktualisiert. Board-Verifikation offen."

---

## Offene Board-Verifikation (nach Umsetzung)

- OLED zeigt Boden-Check, Werte plausibel (GPS-Stufen, SD, Phase).
- Beim Übergang in `Ascent` geht das Display aus (Vext-Abschaltung real?).
- Falls OLED beim V2 NICHT über Vext hängt: `oled_off()` zusätzlich auf reinem
  `setPowerSave(1)` verlassen (Display schläft, auch wenn Rail bleibt).
