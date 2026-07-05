# GPS-Pipeline-Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Die getesteten Logik-Bausteine (parse_gga, TelemetryRecord, FlightPhaseDetector) zu einer laufenden Pipeline verdrahten: `loop()` liest GPS-UART, baut CSV-Zeilen und schreibt sie auf Serial + microSD.

**Architecture:** Der einzige nicht-triviale testbare Kern (UART-Bytes → Zeilen) wird hardware-frei als `LineAssembler` in `lib/telemetry/` gebaut und nativ getestet. Die Board-Verdrahtung (Serial2, SD, millis()) lebt in `src/flight/` und wird am Board verifiziert (🔶).

**Tech Stack:** C++17 (lib, native/Unity), Arduino/ESP32 (src/flight), PlatformIO, Arduino `SD`-Bibliothek.

## Global Constraints

- Sprache: Deutsch (Kommentare, Doku, Commits).
- `lib/telemetry/` ist hardware-frei: KEIN `<Arduino.h>`, reines C++17, Namespace `telemetry`.
- `src/flight/` darf Arduino nutzen; wird am Board verifiziert, nicht nativ getestet.
- TDD verbindlich für `LineAssembler`: erst Test, RED sehen (aus richtigem Grund), dann minimaler Code (GREEN).
- Eigener Test-Ordner `test/test_line_assembler/` mit eigenem `main()` (UNITY_BEGIN/END) — sonst Linker-Kollision.
- Projekt ist **kein** Git-Repo → Commit-Schritte entfallen; stattdessen nach jeder Task Tests laufen lassen.
- Native Tests: `export PATH="$PATH:$HOME/.platformio/penv/bin" && cd /Users/steffenjendrny/weather_balloon && pio test -e native`
- Flight-Build kompilieren (KEIN Board nötig zum Kompilieren): `export PATH="$PATH:$HOME/.platformio/penv/bin" && cd /Users/steffenjendrny/weather_balloon && pio run -e flight`
- `LineAssembler`-Verhalten (verbatim aus Spec): `\n` beendet Zeile (liefert Inhalt ohne `\r`/`\n`); leere Zeile → false; `\r` immer verwerfen; Overflow bei `max_len` (Default 120) → Puffer verwerfen und bis zum nächsten `\n` neu synchronisieren, überlange Zeile komplett verwerfen.
- Pins (aus `src/flight/pins.h`): `PIN_SD_CS` = 13, GPS auf `PIN_GPS_RX`/`PIN_GPS_TX`, `GPS_BAUD` 9600.
- SD: feste Datei `/flight.csv`, Append-Modus; Header (`csv_header()`) nur bei neuer/leerer Datei; bei SD-Fehler no-op + einmal Serial-Warnung, Betrieb läuft weiter.
- Zeit: `t_ms = millis()`. GPS-UTC ist bewusst NICHT Teil dieses Plans (eigener späterer Baustein).

---

### Task 1: `LineAssembler` — Header + einfache Zeile

**Files:**
- Create: `lib/telemetry/line_assembler.h`
- Create: `lib/telemetry/line_assembler.cpp`
- Test: `test/test_line_assembler/test_line_assembler.cpp`

**Interfaces:**
- Consumes: nichts.
- Produces:
  - `class telemetry::LineAssembler` mit `explicit LineAssembler(std::size_t max_len = 120);`
  - `bool push(char c, std::string& out_line);` — true = vollständige, nicht-leere Zeile in out_line (ohne `\r`/`\n`).
  - `void reset();`

- [ ] **Step 1: Header schreiben**

`lib/telemetry/line_assembler.h`:
```cpp
// line_assembler.h — UART-Bytestrom -> vollständige NMEA-Zeilen.
//
// Hardware-frei (kein <Arduino.h>) -> nativ testbar. Nimmt den GPS-UART-Strom
// Byte für Byte entgegen und liefert komplette Zeilen (ohne \r und \n).
// src/flight speist Serial2.read() ein; der Zeilentakt kommt aus '\n'.

#ifndef TELEMETRY_LINE_ASSEMBLER_H
#define TELEMETRY_LINE_ASSEMBLER_H

#include <cstddef>
#include <string>

namespace telemetry {

class LineAssembler {
public:
    explicit LineAssembler(std::size_t max_len = 120);

    // Ein Byte einspeisen. Rückgabe true = eine vollständige, nicht-leere Zeile
    // ist fertig und steht in out_line (ohne \r und \n). Sonst false.
    bool push(char c, std::string& out_line);

    // Puffer verwerfen (z.B. nach einem Fehler).
    void reset();

private:
    std::string buf_;
    std::size_t max_len_;
    bool overflow_ = false;  // true = aktuelle Zeile war zu lang, bis \n verwerfen
};

} // namespace telemetry

#endif // TELEMETRY_LINE_ASSEMBLER_H
```

- [ ] **Step 2: Failing Test für einfache Zeile schreiben**

`test/test_line_assembler/test_line_assembler.cpp`:
```cpp
// test_line_assembler.cpp — native Tests für den Zeilen-Assembler.
//   pio test -e native

#include <unity.h>
#include "line_assembler.h"
#include <string>

using namespace telemetry;

void setUp() {}
void tearDown() {}

// Hilfsroutine: speist einen ganzen String Byte für Byte ein und sammelt
// alle fertigen Zeilen in einem Vektor-Ersatz (hier: durch \x01 getrennt).
// Rückgabe: Anzahl fertiger Zeilen; die Zeilen landen in 'lines'.
static int feed(LineAssembler& a, const std::string& in, std::string lines[], int max_lines) {
    int n = 0;
    for (char c : in) {
        std::string out;
        if (a.push(c, out) && n < max_lines) lines[n++] = out;
    }
    return n;
}

// Einfache Zeile mit \n -> genau eine fertige Zeile ohne Zeilenende.
void test_simple_line() {
    LineAssembler a;
    std::string lines[4];
    int n = feed(a, "ABC\n", lines, 4);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_STRING("ABC", lines[0].c_str());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_simple_line);
    return UNITY_END();
}
```

- [ ] **Step 3: Test laufen lassen — muss FEHLSCHLAGEN**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && cd /Users/steffenjendrny/weather_balloon && pio test -e native`
Expected: FAIL — Linker-Fehler „undefined reference to `telemetry::LineAssembler::push`" (Implementierung fehlt). Erwartetes RED.

- [ ] **Step 4: Minimale Implementierung**

`lib/telemetry/line_assembler.cpp`:
```cpp
// line_assembler.cpp — siehe line_assembler.h. Hardware-frei.

#include "line_assembler.h"

namespace telemetry {

LineAssembler::LineAssembler(std::size_t max_len) : max_len_(max_len) {}

bool LineAssembler::push(char c, std::string& out_line) {
    if (c == '\r') return false;      // CRLF: \r immer verwerfen

    if (c == '\n') {
        if (overflow_) {              // überlange Zeile: verwerfen, resync fertig
            overflow_ = false;
            buf_.clear();
            return false;
        }
        if (buf_.empty()) return false;   // leere Zeile nicht durchreichen
        out_line = buf_;
        buf_.clear();
        return true;
    }

    if (overflow_) return false;      // bis zum nächsten \n weiter verwerfen

    if (buf_.size() >= max_len_) {    // Overflow: Zeile als defekt verwerfen
        overflow_ = true;
        buf_.clear();
        return false;
    }

    buf_.push_back(c);
    return false;
}

void LineAssembler::reset() {
    buf_.clear();
    overflow_ = false;
}

} // namespace telemetry
```

- [ ] **Step 5: Test laufen lassen — muss BESTEHEN**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && cd /Users/steffenjendrny/weather_balloon && pio test -e native`
Expected: PASS (`test_simple_line`), alle anderen Suites weiterhin grün.

---

### Task 2: `LineAssembler` — restliche Verhaltensfälle absichern

**Files:**
- Modify: `test/test_line_assembler/test_line_assembler.cpp`

**Interfaces:**
- Consumes: `LineAssembler` aus Task 1.
- Produces: keine neuen Symbole; vollständige Verhaltensabsicherung.

- [ ] **Step 1: Failing/Absicherungs-Tests schreiben**

In `test/test_line_assembler/test_line_assembler.cpp` vor `main()` ergänzen:
```cpp
// Fragmentiert eingespeist -> gleiches Ergebnis wie am Stück.
void test_fragmented() {
    LineAssembler a;
    std::string out;
    TEST_ASSERT_FALSE(a.push('A', out));
    TEST_ASSERT_FALSE(a.push('B', out));
    TEST_ASSERT_TRUE(a.push('\n', out));
    TEST_ASSERT_EQUAL_STRING("AB", out.c_str());
}

// CRLF: \r wird verworfen, Inhalt bleibt sauber.
void test_crlf_stripped() {
    LineAssembler a;
    std::string lines[4];
    int n = feed(a, "ABC\r\n", lines, 4);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_STRING("ABC", lines[0].c_str());
}

// Zwei Zeilen am Stück -> zwei fertige Zeilen in Reihenfolge.
void test_multiple_lines() {
    LineAssembler a;
    std::string lines[4];
    int n = feed(a, "A\nB\n", lines, 4);
    TEST_ASSERT_EQUAL_INT(2, n);
    TEST_ASSERT_EQUAL_STRING("A", lines[0].c_str());
    TEST_ASSERT_EQUAL_STRING("B", lines[1].c_str());
}

// Leere Zeile (\n ohne Inhalt) -> nichts geliefert.
void test_empty_line_ignored() {
    LineAssembler a;
    std::string out;
    TEST_ASSERT_FALSE(a.push('\n', out));
}

// Overflow: Zeile länger als max_len ohne \n wird komplett verworfen;
// die darauffolgende gültige Zeile kommt sauber durch.
void test_overflow_then_recover() {
    LineAssembler a(4);              // kleiner Puffer für den Test
    std::string lines[4];
    // "ABCDEFG" (7 > 4) -> Overflow; \n schließt die defekte Zeile ab (kein Liefern);
    // dann "OK\n" muss sauber kommen.
    int n = feed(a, "ABCDEFG\nOK\n", lines, 4);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_STRING("OK", lines[0].c_str());
}

// reset() leert einen halb gefüllten Puffer.
void test_reset_clears_partial() {
    LineAssembler a;
    std::string out;
    a.push('X', out);                // halb gefüllt
    a.reset();
    // Nach reset beginnt eine frische Zeile: "Y\n" -> "Y", nicht "XY".
    TEST_ASSERT_FALSE(a.push('Y', out));
    TEST_ASSERT_TRUE(a.push('\n', out));
    TEST_ASSERT_EQUAL_STRING("Y", out.c_str());
}
```
Und in `main()` ergänzen:
```cpp
    RUN_TEST(test_fragmented);
    RUN_TEST(test_crlf_stripped);
    RUN_TEST(test_multiple_lines);
    RUN_TEST(test_empty_line_ignored);
    RUN_TEST(test_overflow_then_recover);
    RUN_TEST(test_reset_clears_partial);
```

- [ ] **Step 2: Tests laufen lassen**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && cd /Users/steffenjendrny/weather_balloon && pio test -e native`
Expected: alle `test_line_assembler`-Tests PASS. Falls `test_overflow_then_recover` fehlschlägt: die Overflow-Logik in `push()` gegen das in den Global Constraints beschriebene Verhalten prüfen (überlange Zeile komplett verwerfen, beim nächsten `\n` resync abschließen, nächste Zeile sauber). Nicht den Testerwartungswert an eine falsche Implementierung anpassen.

---

### Task 3: `sd_log` — microSD-Wrapper (Board, 🔶)

**Files:**
- Create: `src/flight/sd_log.h`
- Create: `src/flight/sd_log.cpp`

**Interfaces:**
- Consumes: `telemetry::csv_header()` aus `lib/telemetry/record.h`; `PIN_SD_CS` aus `pins.h`.
- Produces:
  - `bool sd_log_begin();` — true = Karte bereit; legt bei neuer/leerer `/flight.csv` die Kopfzeile an.
  - `void sd_log(const String& line);` — hängt eine CSV-Zeile an; no-op bei nicht bereiter Karte.

Dieser Baustein ist Hardware-Code (Arduino `SD`) und wird nicht nativ getestet,
sondern kompiliert und am Board verifiziert. Prüfbares Zwischenziel dieser Task:
**der Flight-Build kompiliert fehlerfrei.**

- [ ] **Step 1: Header schreiben**

`src/flight/sd_log.h`:
```cpp
// sd_log.h — microSD-Logging der Telemetrie-CSV (Hardware-Teil).
//
// Dünner Wrapper um die Arduino-SD-Bibliothek (SPI, CS an PIN_SD_CS=13, Bus mit
// LoRa geteilt). Feste Datei /flight.csv im Append-Modus: bei Neustart wird
// weitergeschrieben. Header nur bei neuer/leerer Datei. Am Board zu verifizieren.

#ifndef FLIGHT_SD_LOG_H
#define FLIGHT_SD_LOG_H

#include <Arduino.h>

// Initialisiert die SD-Karte (SPI, CS = PIN_SD_CS). Rückgabe: true = bereit.
// Legt bei neuer/leerer /flight.csv die CSV-Kopfzeile (csv_header()) an.
bool sd_log_begin();

// Hängt eine CSV-Zeile (ohne Zeilenende) an /flight.csv an.
// Bei nicht initialisierter/fehlerhafter Karte: no-op.
void sd_log(const String& line);

#endif // FLIGHT_SD_LOG_H
```

- [ ] **Step 2: Implementierung schreiben**

`src/flight/sd_log.cpp`:
```cpp
// sd_log.cpp — siehe sd_log.h. Hardware-Teil, am Board verifizieren.

#include "sd_log.h"
#include <SD.h>
#include <SPI.h>
#include "pins.h"
#include "record.h"   // telemetry::csv_header()

static bool s_ready = false;
static const char* kLogPath = "/flight.csv";

bool sd_log_begin() {
    if (!SD.begin(PIN_SD_CS)) {
        Serial.println("[flight] !!! microSD nicht gefunden — Logging aus, Betrieb laeuft !!!");
        s_ready = false;
        return false;
    }
    s_ready = true;

    // Header nur schreiben, wenn Datei neu oder leer ist (kein Doppel-Header bei Append).
    bool need_header = true;
    if (SD.exists(kLogPath)) {
        File f = SD.open(kLogPath, FILE_READ);
        if (f) {
            if (f.size() > 0) need_header = false;
            f.close();
        }
    }
    if (need_header) {
        File f = SD.open(kLogPath, FILE_APPEND);
        if (f) {
            f.println(telemetry::csv_header().c_str());
            f.close();
        }
    }
    Serial.println("[flight] microSD bereit — Log: /flight.csv");
    return true;
}

void sd_log(const String& line) {
    if (!s_ready) return;
    File f = SD.open(kLogPath, FILE_APPEND);
    if (!f) return;
    f.println(line);
    f.close();
}
```

- [ ] **Step 3: Flight-Build kompilieren — muss fehlerfrei sein**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && cd /Users/steffenjendrny/weather_balloon && pio run -e flight`
Expected: SUCCESS. Falls `FILE_APPEND` unbekannt ist (ältere SD-Lib): Datei mit `SD.open(kLogPath, FILE_WRITE)` öffnen — bei der ESP32-SD-Lib positioniert `FILE_WRITE` ans Dateiende (append). In diesem Fall beide Vorkommen ersetzen und erneut kompilieren.

---

### Task 4: `loop()` verdrahten — Pipeline in `main.cpp` (Board, 🔶)

**Files:**
- Modify: `src/flight/main.cpp`

**Interfaces:**
- Consumes: `telemetry::LineAssembler` (Task 1), `telemetry::parse_gga`/`GpsFix` (`gga.h`), `telemetry::TelemetryRecord`/`csv_row` (`record.h`), `telemetry::FlightPhaseDetector` (`flight_phase.h`), `sd_log_begin`/`sd_log` (Task 3).
- Produces: kein neues Symbol; die laufende Pipeline.

Prüfbares Zwischenziel: **Flight-Build kompiliert fehlerfrei.** Das Laufzeit-
verhalten wird am Board verifiziert (🔶).

- [ ] **Step 1: `main.cpp` ersetzen**

`src/flight/main.cpp` vollständig ersetzen durch:
```cpp
// main.cpp — Flug-Einheit (Heltec WiFi LoRa 32 V2)
//
// Stand: GPS-Pipeline. setup() setzt GPS-Flight-Mode + initialisiert SD.
// loop() liest den GPS-UART, baut pro GGA-Satz eine Telemetrie-CSV-Zeile
// (parse_gga -> TelemetryRecord -> Flugphase -> csv_row) und schreibt sie auf
// Serial und microSD. Testbare Logik lebt in lib/telemetry (nativ getestet).

#include <Arduino.h>
#include "pins.h"
#include "gps_flightmode.h"
#include "sd_log.h"

#include "line_assembler.h"
#include "gga.h"
#include "record.h"
#include "flight_phase.h"

using namespace telemetry;

static LineAssembler       g_asm;
static TelemetryRecord     g_rec;
static FlightPhaseDetector g_detector;

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println("[flight] Wetterballon Flug-Einheit — Boot");

    // GPS an UART2 starten (NEO-6M, 9600 Baud, RX23/TX17 lt. Pin-Spec).
    Serial2.begin(GPS_BAUD, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
    Serial.println("[flight] GPS UART2 @9600 gestartet");

    // KRITISCH: Höhenflug-Modus setzen, sonst GPS-Abschaltung > ~18 km.
    Serial.println("[flight] Setze GPS Flight-Mode (Dynamic Model 6)...");
    if (set_gps_flight_mode(Serial2)) {
        Serial.println("[flight] >>> GPS Flight-Mode BESTAETIGT (ACK) <<<");
    } else {
        Serial.println("[flight] !!! GPS Flight-Mode NICHT bestaetigt — pruefen! !!!");
    }

    // microSD initialisieren (Logging optional — Betrieb läuft auch ohne).
    sd_log_begin();

    // CSV-Kopfzeile einmal auf Serial ausgeben (Orientierung im Monitor).
    Serial.println(csv_header().c_str());
}

void loop() {
    while (Serial2.available()) {
        char c = static_cast<char>(Serial2.read());
        std::string line;
        if (!g_asm.push(c, line)) continue;

        GpsFix fix;
        if (!parse_gga(line, fix)) continue;   // Nicht-GGA / kaputt -> überspringen

        g_rec.t_ms      = millis();
        g_rec.has_fix   = fix.has_fix;
        g_rec.lat       = fix.lat;
        g_rec.lon       = fix.lon;
        g_rec.alt_gps_m = fix.alt_gps_m;
        g_rec.sats      = fix.sats;

        if (fix.has_fix) {
            g_rec.phase = g_detector.update(fix.alt_gps_m, g_rec.t_ms);
        } else {
            g_rec.phase = g_detector.phase();  // ohne Fix letzte Phase halten
        }

        String csv = csv_row(g_rec).c_str();
        Serial.println(csv);
        sd_log(csv);
    }
}
```

- [ ] **Step 2: Prüfen, dass `lib`-Header im Flight-Build sichtbar sind**

`platformio.ini` `[env:flight]` erbt `esp32_base`. Bibliotheken in `lib/telemetry/`
werden vom PlatformIO Library Dependency Finder automatisch gefunden und
mitkompiliert (LDF durchsucht `lib/`). Keine `platformio.ini`-Änderung nötig —
nur verifizieren, dass `line_assembler`, `gga`, `record`, `flight_phase` in der
Build-Ausgabe als kompilierte Objekte erscheinen.

- [ ] **Step 3: Flight-Build kompilieren — muss fehlerfrei sein**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && cd /Users/steffenjendrny/weather_balloon && pio run -e flight`
Expected: SUCCESS (Compile + Link). Warnungen zu ungenutzten Feldern sind ok; Fehler nicht.

- [ ] **Step 4: Native Suite erneut laufen lassen — nichts kaputt gemacht**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && cd /Users/steffenjendrny/weather_balloon && pio test -e native`
Expected: alle nativen Tests (inkl. `test_line_assembler`) PASS.

---

### Task 5: `docs/software-flow.md` aktualisieren

**Files:**
- Modify: `docs/software-flow.md`

**Interfaces:**
- Consumes: nichts. Produces: aktualisierte Doku.

- [ ] **Step 1: §3 loop()-Diagramm aktualisieren**

Die Knoten des `loop()`-Diagramms auf den neuen Stand bringen:
- `READ_GPS` (bereits ✅ aus dem letzten Schritt) bleibt ✅.
- Der Zusammenbau `BUILD` (TelemetryRecord befüllen) und `CSV` (csv_row) sind
  jetzt in `main.cpp` verdrahtet → als 🔶 „geschrieben, Board-Test offen"
  markieren (gelb, `fill:#fff3cd`), da am Board unverifiziert.
- `SD` (Auf microSD schreiben) von ⬜ auf 🔶 (gelb) setzen.
- `READ_SENS` (Sensoren) und `LORA` bleiben ⬜ (grau).
- Zwischen `READ_GPS` und `BUILD` gehört nun der Zeilen-Assembler: entweder als
  Hinweis am `READ_GPS`-Knoten (`✅ LineAssembler + parse_gga → GpsFix`) ergänzen.

- [ ] **Step 2: §6-Tabelle ergänzen/aktualisieren**

- Neue Zeile: `| NMEA-Zeilen-Assembler | lib/telemetry/line_assembler | ✅ nativ getestet |` (direkt nach der NMEA-Parser-Zeile einsortieren).
- Zeile `microSD-Logging` von `⬜` auf `🔶 geschrieben, Board-Test offen`, Ort `src/flight/sd_log`.
- Falls eine Zeile für die loop()-Pipeline/main.cpp sinnvoll ist, ergänzen: `| GPS-Pipeline in loop() | src/flight/main.cpp | 🔶 geschrieben, Board-Test offen |`.

- [ ] **Step 3: §6 Schlussabsatz anpassen**

Den Schlussabsatz um den neuen Stand ergänzen: Zeilen-Assembler ist fertig &
nativ getestet; die GPS-Pipeline (bis SD) ist verdrahtet und wartet auf die
Board-Verifikation (erster Punkt der TODO-Testreihenfolge). Als nächsten
testbaren `lib`-Baustein die geplante **GPS-UTC-Spalte** nennen (siehe
`docs/superpowers/specs/2026-07-04-gps-pipeline-integration-design.md`,
Abschnitt „Zeit & Synchronisierung").

- [ ] **Step 4: Sichtprüfung**

Diagramm-Marker, Tabelle und Text konsistent; kein Baustein ist ✅, der nicht
nativ getestet ist (SD-Logging und loop()-Pipeline sind 🔶, nicht ✅).

---

## Self-Review

**Spec coverage:**
- LineAssembler (Verhalten + 7 Testfälle) ✅ Task 1+2 (simple, fragmentiert, CRLF, mehrere, leer, overflow, reset).
- loop()-Verdrahtung mit millis(), Phase nur bei Fix, Nicht-GGA überspringen ✅ Task 4.
- microSD: feste Datei /flight.csv, Append, Header nur einmal, Robustheit/no-op ✅ Task 3.
- setup()-Ergänzung (sd_log_begin + Log) ✅ Task 4 Step 1.
- Zeit-Entscheidung millis(), UTC nicht Teil ✅ (Global Constraints + kein UTC-Task); als nächster Schritt in Doku genannt (Task 5 Step 3).
- Software-Flow nachpflegen ✅ Task 5.

**Placeholder scan:** Keine TBD/TODO; jeder Code-Schritt zeigt vollständigen Code. Der FILE_APPEND/FILE_WRITE-Hinweis in Task 3 Step 3 ist eine konkrete, benannte Fallback-Anweisung, kein Platzhalter.

**Type consistency:**
- `LineAssembler(std::size_t max_len=120)`, `push(char, std::string&)->bool`, `reset()` durchgehend gleich in Task 1/2/4.
- `sd_log_begin()->bool`, `sd_log(const String&)` gleich in Task 3/4.
- Record-Felder (`t_ms, phase, has_fix, lat, lon, alt_gps_m, sats`) stimmen mit `record.h` überein; `GpsFix`-Felder (`has_fix, lat, lon, alt_gps_m, sats`) mit `gga.h`; `FlightPhaseDetector.update(float,uint32_t)`/`.phase()` mit `flight_phase.h`.
- `csv_header()`/`csv_row()` Signaturen (Rückgabe `std::string`) → in Arduino via `.c_str()` in `String` gewandelt, wie in Task 3/4 gezeigt.

**Hinweis Board-Verifikation:** Tasks 3 und 4 sind Hardware-Code; ihr prüfbares Zwischenziel im Plan ist der erfolgreiche `pio run -e flight`-Kompilat. Das echte Laufzeitverhalten (CSV über Serial, /flight.csv-Inhalt, SD-Versorgung 3,3/5 V) gehört in die TODO-Board-Testreihenfolge und ist bewusst nicht Teil dieses nativ prüfbaren Plans.
