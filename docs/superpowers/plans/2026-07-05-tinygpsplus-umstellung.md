# GPS-Auslese auf TinyGPSPlus umstellen — Implementierungsplan

> **Für agentische Worker:** ERFORDERLICHE SUB-SKILL: superpowers:subagent-driven-development (empfohlen) oder superpowers:executing-plans, um diesen Plan Aufgabe für Aufgabe umzusetzen. Schritte nutzen Checkbox-Syntax (`- [ ]`) zur Nachverfolgung.

**Goal:** Den selbstgebauten NMEA-Parser (`gga` + `line_assembler`) durch die Bibliothek TinyGPSPlus ersetzen und das Logging von satzgetrieben auf zeitgesteuert (1 Hz) umstellen.

**Architecture:** TinyGPSPlus (Arduino-gebunden) zieht in einen dünnen Wrapper `src/flight/gps_reader`, der das GPS-Objekt kapselt, den UART-Strom füttert und den `TelemetryRecord` befüllt. `main.cpp` ruft nur noch `gps_feed()`, `gps_fill()` und `gps_display_state()`. Das hardware-freie `lib/telemetry` verliert die beiden GPS-Parser-Dateien; CSV-Format und UBX-Flight-Mode bleiben unverändert.

**Tech Stack:** C++ (Arduino-Framework, ESP32), PlatformIO, TinyGPSPlus (`mikalhart/TinyGPSPlus`, bereits in `platformio.ini`).

## Global Constraints

- Sprache im Projekt: **Deutsch** (Code-Kommentare, Commits, Doku).
- **Keine automatisierten Tests.** Verifiziert wird durch Kompilieren (`pio run -e flight`) und Verhalten am `pio device monitor`. PATH ggf. setzen: `export PATH="$PATH:$HOME/.platformio/penv/bin"`.
- **YAGNI strikt:** keine Sensor-Platzhalter, keine Konfig auf Vorrat.
- CSV-/Telemetrie-Format (`TelemetryRecord`, `csv_header`, `csv_row`, `parse_csv_row`) bleibt **unverändert** — keine Spalte ändert sich.
- Flight-Mode (`src/flight/gps_flightmode.*`, `lib/telemetry/ubx`) bleibt **unberührt**.
- Board Flug-Einheit: Heltec WiFi LoRa 32 V2. GPS an UART2: RX 23, TX 17, 9600 Baud.

---

### Task 1: gps_reader-Wrapper anlegen

Neuer Arduino-gebundener Wrapper, der TinyGPSPlus kapselt. Danach kompiliert das
Projekt noch NICHT über diesen Weg (main.cpp nutzt ihn erst in Task 2) — deshalb
wird hier isoliert geprüft, dass die neue Übersetzungseinheit für sich fehlerfrei
kompiliert, indem sie vorübergehend gegen das noch bestehende main.cpp mitgebaut
wird. (Der eigentliche Verdrahtungs-Build kommt in Task 2.)

**Files:**
- Create: `src/flight/gps_reader.h`
- Create: `src/flight/gps_reader.cpp`

**Interfaces:**
- Consumes: `telemetry::TelemetryRecord` (aus `record.h`), `telemetry::GpsDisp` (aus `display_status.h`), `TinyGPSPlus` (aus Library).
- Produces:
  - `void gps_feed(Stream& gps);`
  - `void gps_fill(telemetry::TelemetryRecord& r);`
  - `telemetry::GpsDisp gps_display_state();`

- [ ] **Step 1: Header schreiben**

Create `src/flight/gps_reader.h`:

```cpp
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
```

- [ ] **Step 2: Implementierung schreiben**

Create `src/flight/gps_reader.cpp`:

```cpp
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
```

- [ ] **Step 3: Isoliert kompilieren**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin"; pio run -e flight`
Expected: **Kompiliert fehlerfrei.** Das alte main.cpp nutzt den Wrapper noch
nicht, aber `gps_reader.cpp` wird mitgebaut und muss sauber übersetzen
(insb. `#include <TinyGPS++.h>` wird gefunden, Signaturen stimmen). Es darf keine
Warnung/kein Fehler aus `gps_reader.*` kommen.

- [ ] **Step 4: Commit**

```bash
git add src/flight/gps_reader.h src/flight/gps_reader.cpp
git commit -m "feat(flight): gps_reader — TinyGPSPlus-Wrapper (feed/fill/display_state)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 2: main.cpp auf gps_reader umstellen

`main.cpp` von satzgetrieben (`LineAssembler` + `parse_gga`) auf zeitgesteuertes
Logging über `gps_reader` umbauen.

**Files:**
- Modify: `src/flight/main.cpp`

**Interfaces:**
- Consumes: `gps_feed(Stream&)`, `gps_fill(TelemetryRecord&)`, `gps_display_state()` (aus Task 1).

- [ ] **Step 1: Includes und globale Objekte anpassen**

In `src/flight/main.cpp` die GPS-Parser-Includes entfernen und den Wrapper aufnehmen.

Entfernen:
```cpp
#include "line_assembler.h"
#include "gga.h"
```
Hinzufügen (bei den anderen `src/flight`-Includes, z.B. nach `#include "sd_log.h"`):
```cpp
#include "gps_reader.h"
```

Globale Objekte anpassen — diesen Block:
```cpp
static LineAssembler       g_asm;
static TelemetryRecord     g_rec;
static FlightPhaseDetector g_detector;
static bool g_sd_ok       = false;  // Ergebnis von sd_log_begin(), fürs Display
static bool g_bmp_ok      = false;  // Ergebnis von bmp_begin(), fürs Display
static bool g_gps_seen    = false;  // schon je eine GGA geparst? (GPS-Stufe)
static bool g_oled_active = true;   // Display läuft, bis PreFlight verlassen wird
static uint32_t g_last_oled_ms = 0;              // letzte OLED-Aktualisierung
static const uint32_t OLED_REFRESH_MS = 500;     // OLED höchstens alle 500 ms neu zeichnen
```
ersetzen durch:
```cpp
static TelemetryRecord     g_rec;
static FlightPhaseDetector g_detector;
static bool g_sd_ok       = false;  // Ergebnis von sd_log_begin(), fürs Display
static bool g_bmp_ok      = false;  // Ergebnis von bmp_begin(), fürs Display
static bool g_oled_active = true;   // Display läuft, bis PreFlight verlassen wird
static uint32_t g_last_oled_ms = 0;              // letzte OLED-Aktualisierung
static const uint32_t OLED_REFRESH_MS = 500;     // OLED höchstens alle 500 ms neu zeichnen
static uint32_t g_last_log_ms = 0;               // letzte CSV-Zeile geschrieben
static const uint32_t LOG_INTERVAL_MS = 1000;    // eine CSV-Zeile pro Sekunde (1 Hz)
```
(`g_asm` und `g_gps_seen` entfallen; `g_last_log_ms`/`LOG_INTERVAL_MS` kommen hinzu.)

- [ ] **Step 2: Datei-Kommentarkopf aktualisieren**

Den einleitenden Kommentar von `main.cpp` (Absatz „loop() liest den GPS-UART …")
an das neue Verhalten anpassen. Ersetzen:
```cpp
// Stand: GPS-Pipeline. setup() setzt GPS-Flight-Mode + initialisiert SD.
// loop() liest den GPS-UART, baut pro GGA-Satz eine Telemetrie-CSV-Zeile
// (parse_gga -> TelemetryRecord -> Flugphase -> csv_row) und schreibt sie auf
// Serial und microSD. Testbare Logik lebt in lib/telemetry (nativ getestet).
```
durch:
```cpp
// Stand: GPS-Pipeline. setup() setzt GPS-Flight-Mode + initialisiert SD.
// loop() füttert den GPS-UART laufend an TinyGPSPlus (gps_reader) und schreibt
// einmal pro Sekunde eine Telemetrie-CSV-Zeile aus dem aktuellen GPS-Zustand
// (gps_fill -> TelemetryRecord -> Flugphase -> csv_row) auf Serial und microSD.
```

- [ ] **Step 3: loop() umbauen**

Den gesamten bisherigen `while (Serial2.available()) { ... }`-Block ersetzen.
Diesen Block:
```cpp
    while (Serial2.available()) {
        char c = static_cast<char>(Serial2.read());
        std::string line;
        if (!g_asm.push(c, line)) continue;

        GpsFix fix;
        if (!parse_gga(line, fix)) continue;   // Nicht-GGA / kaputt -> überspringen

        g_gps_seen = true;

        g_rec.t_ms      = millis();
        g_rec.has_utc   = fix.has_utc;
        g_rec.utc_h     = fix.utc_h;
        g_rec.utc_min   = fix.utc_min;
        g_rec.utc_s     = fix.utc_s;
        g_rec.has_fix     = fix.has_fix;
        g_rec.fix_quality = fix.fix_quality;
        g_rec.lat       = fix.lat;
        g_rec.lon       = fix.lon;
        g_rec.alt_gps_m = fix.alt_gps_m;
        g_rec.sats      = fix.sats;

        if (fix.has_fix) {
            g_rec.phase = g_detector.update(fix.alt_gps_m, g_rec.t_ms);
        } else {
            g_rec.phase = g_detector.phase();  // ohne Fix letzte Phase halten
        }

        bmp_read(g_rec);

        String csv = csv_row(g_rec).c_str();
        Serial.println(csv);
        sd_log(csv);
    }
```
ersetzen durch:
```cpp
    // GPS laufend füttern (nicht-blockierend): TinyGPSPlus sammelt + parst.
    gps_feed(Serial2);

    // Zeitgesteuert: einmal pro Sekunde eine CSV-Zeile aus dem aktuellen Zustand.
    if (millis() - g_last_log_ms >= LOG_INTERVAL_MS) {
        g_last_log_ms = millis();

        g_rec.t_ms = millis();
        gps_fill(g_rec);   // GPS-Felder aus TinyGPSPlus in den Record

        if (g_rec.has_fix) {
            g_rec.phase = g_detector.update(g_rec.alt_gps_m, g_rec.t_ms);
        } else {
            g_rec.phase = g_detector.phase();  // ohne Fix letzte Phase halten
        }

        bmp_read(g_rec);

        String csv = csv_row(g_rec).c_str();
        Serial.println(csv);
        sd_log(csv);
    }
```

- [ ] **Step 4: OLED-Block auf gps_display_state() umstellen**

Im OLED-Block die `DisplayState`-Befüllung anpassen. Diese Zeilen:
```cpp
            DisplayState ds;
            ds.gps   = g_rec.has_fix ? GpsDisp::Fix
                     : g_gps_seen    ? GpsDisp::Waiting
                                     : GpsDisp::Silent;
            ds.sats  = g_rec.sats;
```
ersetzen durch:
```cpp
            DisplayState ds;
            ds.gps   = gps_display_state();
            ds.sats  = g_rec.sats;
```
(Der Rest des OLED-Blocks — `ds.sd_ok`, `ds.bmp_ok`, `ds.phase`, `oled_show`,
`oled_off` — bleibt unverändert.)

- [ ] **Step 5: Kompilieren**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin"; pio run -e flight`
Expected: **Kompiliert fehlerfrei.** Keine Referenzen mehr auf `g_asm`,
`g_gps_seen`, `LineAssembler`, `GpsFix`, `parse_gga`. (Da `gga.*`/`line_assembler.*`
noch existieren, aber nicht mehr inkludiert werden, gibt es keinen Fehler — sie
werden in Task 3 gelöscht.)

- [ ] **Step 6: Verhalten am Board prüfen**

Board flashen und seriell mitlesen:
Run: `export PATH="$PATH:$HOME/.platformio/penv/bin"; pio run -e flight -t upload && pio device monitor`
Erwartetes Verhalten:
- Boot-Ausgaben wie bisher (Flight-Mode-ACK-Zeile).
- Danach **etwa jede Sekunde** eine CSV-Zeile im bekannten Format
  (`t_ms,utc,phase,fix_q,lat,lon,alt_gps_m,sats,...`).
- Ohne Fix: `fix_q`=0, lat/lon/alt-Felder leer, `utc` sobald das GPS Zeit liefert.
- OLED zeigt am Boden `GPS: --` (Silent) bzw. `warte`/`Fix` je nach Empfang.

- [ ] **Step 7: Commit**

```bash
git add src/flight/main.cpp
git commit -m "feat(flight): main.cpp auf TinyGPSPlus-Wrapper + 1-Hz-Logging umstellen

Satzgetriebenes Logging (pro GGA) ersetzt durch zeitgesteuertes (1 Hz)
aus dem aktuellen GPS-Zustand. LineAssembler/parse_gga/g_gps_seen entfernt.

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 3: Alten Eigenbau-Parser löschen

Die nun ungenutzten hardware-freien Parser-Dateien entfernen.

**Files:**
- Delete: `lib/telemetry/gga.h`
- Delete: `lib/telemetry/gga.cpp`
- Delete: `lib/telemetry/line_assembler.h`
- Delete: `lib/telemetry/line_assembler.cpp`

- [ ] **Step 1: Sicherstellen, dass nichts mehr darauf verweist**

Run: `grep -rn "gga.h\|line_assembler\|parse_gga\|LineAssembler\|GpsFix" lib/ src/`
Expected: **Keine Treffer** (außer ggf. in Kommentaren, die dann mit-bereinigt
werden). Falls ein Code-Treffer erscheint, zuerst dort beheben.

- [ ] **Step 2: Dateien löschen**

```bash
git rm lib/telemetry/gga.h lib/telemetry/gga.cpp \
       lib/telemetry/line_assembler.h lib/telemetry/line_assembler.cpp
```

- [ ] **Step 3: Kompilieren**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin"; pio run -e flight`
Expected: **Kompiliert fehlerfrei** — die gelöschten Dateien wurden von niemandem
mehr gebraucht.

- [ ] **Step 4: Commit**

```bash
git commit -m "refactor(telemetry): Eigenbau-NMEA-Parser (gga + line_assembler) entfernen

Ersetzt durch TinyGPSPlus (src/flight/gps_reader).

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 4: Dokumentation nachziehen

Software-Flow, überholten Plan und CLAUDE.md-Projektstruktur an den neuen Stand
anpassen.

**Files:**
- Modify: `docs/software-flow.md`
- Modify: `docs/superpowers/plans/2026-07-04-nmea-gga-parser.md`
- Modify: `CLAUDE.md`

- [ ] **Step 1: software-flow.md aktualisieren**

`docs/software-flow.md` öffnen und den GPS-Baustein anpassen: Wo heute
`line_assembler` → `gga`/`parse_gga` im Diagramm/Fließtext steht, durch
„TinyGPSPlus (gps_reader: feed → fill)" ersetzen. Den Schreibtakt von
„pro GGA-Satz" auf „zeitgesteuert 1 Hz" ändern. Status-Marker (✅/🔶/⬜) prüfen
und, falls der GPS-Weg als ✅ markiert war, auf den neuen Stand setzen (Umbau am
Board verifiziert = ✅, sonst 🔶). Exakte Textstellen ergeben sich beim Öffnen der
Datei; nur den GPS-Abschnitt anfassen.

- [ ] **Step 2: Überholten Parser-Plan markieren**

Am Anfang von `docs/superpowers/plans/2026-07-04-nmea-gga-parser.md` einen
Hinweis-Block einfügen (nach der Überschrift):
```markdown
> **⚠️ ÜBERHOLT (2026-07-05):** Der hier beschriebene Eigenbau-Parser
> (`gga` + `line_assembler`) wurde durch TinyGPSPlus ersetzt. Siehe
> `docs/superpowers/specs/2026-07-05-tinygpsplus-umstellung-design.md`.
> Dieser Plan ist nur noch historischer Kontext.
```

- [ ] **Step 3: CLAUDE.md-Projektstruktur bereinigen**

In `CLAUDE.md` im Projektstruktur-Block die beiden Zeilen für den GPS-Parser in
`lib/telemetry/` anpassen. Die Zeilen
```
  flight_phase.h/.cpp     #   Flugphasen-Automat (PreFlight/Ascent/Descent/Landed)
  ubx.h/.cpp              #   UBX-CFG-NAV5 bauen + Checksumme + ACK-Parsing
  record.h/.cpp           #   Telemetrie-/CSV-Format (SD + LoRa, gemeinsam)
```
bleiben; falls `gga`/`line_assembler` dort (oder unter `src/flight/`) gelistet
sind, entfernen und stattdessen unter `src/flight/` `gps_reader.h/.cpp`
(„GPS-Auslese über TinyGPSPlus") ergänzen. Beim Öffnen der Datei die exakte
Formulierung an den vorhandenen Stil anpassen.

- [ ] **Step 4: Commit**

```bash
git add docs/software-flow.md docs/superpowers/plans/2026-07-04-nmea-gga-parser.md CLAUDE.md
git commit -m "docs: Software-Flow, Parser-Plan und Projektstruktur auf TinyGPSPlus nachziehen

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Selbstprüfung (vom Autor durchgeführt)

- **Spec-Abdeckung:** gps_reader-Wrapper (Task 1) ✓, loop-Umbau + 1-Hz + OLED (Task 2) ✓, Löschen gga/line_assembler (Task 3) ✓, platformio.ini bewusst unverändert (Global Constraints/Design) ✓, Doku-Nachzug (Task 4) ✓. CSV-Format & Flight-Mode unangetastet ✓.
- **Platzhalter:** keine TBD/TODO; Code in jedem Code-Schritt vollständig.
- **Typkonsistenz:** `gps_feed`/`gps_fill`/`gps_display_state` in Task 1 definiert, in Task 2 identisch verwendet. `GpsDisp` aus `display_status.h`, `TelemetryRecord` aus `record.h` — beide existieren.
