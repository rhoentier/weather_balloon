# Report — Task 3 + Task 4: sd_log + loop()-Verdrahtung

**Bezug:** `docs/superpowers/plans/2026-07-04-gps-pipeline-integration.md`, Task 3
("sd_log — microSD-Wrapper") und Task 4 ("loop() verdrahten — Pipeline in main.cpp").

## Erstellte/geänderte Dateien

- **Neu:** `src/flight/sd_log.h` — Wrapper-Interface (`sd_log_begin()`, `sd_log(const String&)`),
  exakt wie im Plan (Task 3 Step 1).
- **Neu:** `src/flight/sd_log.cpp` — Implementierung mit `SD`/`SPI`, feste Datei
  `/flight.csv`, Header nur bei neuer/leerer Datei, Append via `FILE_APPEND`,
  Fehlerpfad = Serial-Warnung + `s_ready=false` (no-op bei `sd_log()`), exakt wie
  im Plan (Task 3 Step 2).
- **Geändert:** `src/flight/main.cpp` — komplett ersetzt durch die im Plan
  vorgegebene Version (Task 4 Step 1): `setup()` initialisiert GPS-UART,
  GPS-Flight-Mode, `sd_log_begin()`, gibt `csv_header()` einmal aus; `loop()`
  liest `Serial2` byteweise über `LineAssembler`, parst GGA-Zeilen, befüllt
  `TelemetryRecord`, aktualisiert die Flugphase nur bei `has_fix` (sonst
  `detector.phase()` gehalten), schreibt `csv_row()` auf Serial und SD.

Keine weiteren Dateien angefasst. `platformio.ini` musste nicht geändert werden
(LDF findet `lib/telemetry/` automatisch; Arduino-`SD`/`SPI` sind Teil des
ESP32-Arduino-Frameworks, kein zusätzlicher `lib_deps`-Eintrag nötig).

## FILE_APPEND-Fallback

**Nicht nötig.** `FILE_APPEND` war in der ESP32-`SD`-Bibliothek bekannt, der
Code aus dem Plan kompilierte unverändert durch. Kein `FILE_WRITE`-Ersatz
vorgenommen.

## Kompilat-Ergebnis

Kommando:
```
export PATH="$PATH:$HOME/.platformio/penv/bin" && cd /Users/steffenjendrny/weather_balloon && pio run -e flight
```
Ergebnis: **SUCCESS** (Compile + Link), 6,62 s. `libtelemetry.a` enthält
`flight_phase.cpp.o`, `gga.cpp.o`, `line_assembler.cpp.o`, `record.cpp.o`,
`ubx.cpp.o` — alle vier von Task 4 benötigten `lib/telemetry`-Bausteine wurden
mitkompiliert und gelinkt. RAM 6,8 % (22200/327680 B), Flash 10,6 %
(352933/3342336 B). Keine Compiler-Fehler; nur harmlose, vorbestehende
`#undef`-Warnungen aus der `OneWire`-Bibliothek (nicht Teil dieser Änderung).

## Native-Testergebnis

Kommando:
```
export PATH="$PATH:$HOME/.platformio/penv/bin" && cd /Users/steffenjendrny/weather_balloon && pio test -e native
```
Ergebnis: **39 von 39 Tests grün** (test_flight_phase 6, test_record 7,
test_line_assembler 8, test_gga 11, test_ubx 7). Nichts kaputt gemacht durch
die Board-Änderungen (erwartungsgemäß, da `src/flight/` nicht Teil des
`native`-Environments ist).

## Bedenken

- Task 3/4 sind reiner Hardware-Code und wie im Plan vorgesehen **nur bis zum
  erfolgreichen Kompilat geprüft**, nicht am echten Board. Laufzeitverhalten
  (SD-Header-Logik bei vorhandener/leerer/fehlender Karte, CSV-Ausgabe bei
  echten NMEA-Sätzen) steht weiterhin in der TODO-Testreihenfolge aus.
- `sd_log_begin()` prüft `f.size() > 0`, um einen Doppel-Header zu vermeiden —
  das deckt den Normalfall ab, aber falls eine vorhandene Datei mit einer
  unvollständigen/abgeschnittenen letzten Zeile endet, wird trotzdem kein neuer
  Header geschrieben (so auch im Plan spezifiziert; kein Abweichen nötig, nur
  zur Kenntnis für die Board-Verifikation).
- Task 5 (Aktualisierung `docs/software-flow.md`) ist laut Auftrag nicht Teil
  dieser Arbeit und macht jemand anderes.

## Important-Fix: Header-Once unter I/O-Fehler

**Problem:** In `sd_log_begin()` startete `need_header` auf `true` und wurde
nur auf `false` gesetzt, wenn sowohl `SD.exists()` als auch der nachfolgende
`SD.open(kLogPath, FILE_READ)` erfolgreich waren UND `f.size() > 0`. Schlug der
`FILE_READ`-Open transient fehl (realistisch: SPI-Bus geteilt mit LoRa —
bekannte Stolperfalle), blieb `need_header=true`, obwohl `/flight.csv` bereits
Flugdaten enthielt → ein zweiter Header wäre mitten in die bestehende Datei
geschrieben worden. Verstößt gegen "kein Doppel-Header bei Append" und
korrumpiert das CSV.

**Fix (defensives Prinzip — im Zweifel keinen Header, um Doppel-Header in
bestehende Daten zu vermeiden):**
- `SD.exists(kLogPath)` == false → Datei existiert nicht → `need_header = true`.
- Datei existiert → `need_header` default `false`. Nur wenn sich die Datei
  öffnen lässt UND `f.size() == 0` (sicher leer, z.B. Absturz vor erstem
  Write) → doch `need_header = true`. Lässt sie sich nicht öffnen (transienter
  Fehler) → bleibt `need_header = false`.

Damit kann ein transienter Lesefehler nie mehr einen Doppel-Header mitten in
Flugdaten erzeugen — im schlimmsten Fall fehlt einmal der Header (harmlos,
nachträglich erkennbar), statt dass die Datei korrumpiert wird.

**Kompilat-Ergebnis:**
```
export PATH="$PATH:$HOME/.platformio/penv/bin" && cd /Users/steffenjendrny/weather_balloon && pio run -e flight
```
Ergebnis: **SUCCESS** (3,72 s). RAM 6,8 % (22200/327680 B), Flash 10,6 %
(352929/3342336 B). Zusätzlich `pio test -e native`: **39/39 grün** (Board-Datei
nicht Teil des native-Environments, aber schadet nicht zu prüfen).
