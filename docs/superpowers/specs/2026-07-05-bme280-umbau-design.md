# Design: BMP280 → BME280 (Sensor-Austausch)

Datum: 2026-07-05
Status: abgesegnet, bereit für Implementierungsplan

## Zweck & Anlass

Im Projekt ist bisher durchgängig vom **BMP280** (Temperatur + Druck, keine
Feuchte) die Rede — inklusive einem fertigen, nativ getesteten Umrechnungs-
modul `lib/telemetry/bmp280`. Tatsächlich verbaut ist aber ein **BME280**
(zusätzlich **Luftfeuchtigkeit**). Dieses Design ersetzt den Platzhalter-
Sensor durchgängig im Projekt: Umrechnung, CSV-Format, Display-Status,
Pin-Doku, Software-Flow — und liest ihn erstmals real am Board aus (I²C).

## Bewusster Architektur-Bruch

Bisher galt die Faustregel aus `CLAUDE.md`: fehleranfällige, kritische
Sensor-Formeln (Bit-Shifts, Vorzeichen, Q-Formate) werden hardware-frei
nachgebaut und nativ gegen verifizierte Referenzwerte getestet — genau das
war die Begründung für `lib/telemetry/bmp280` (GPS-UBX-Bytes folgen demselben
Muster).

Für den BME280 wird davon **bewusst abgewichen**: Wir nutzen die fertige
**Adafruit BME280 Library**, die I²C-Kommunikation *und* Kompensations-
rechnung intern übernimmt. Der Sensor-Teil ist damit eine Black Box ohne
eigenen nativen Test. Grund: der bereits gebaute `bmp280`-Baustein war für
den *falschen* Sensor (keine Feuchte-Formel vorhanden), eine eigene
BME280-Kompensation neu zu bauen wäre deutlich mehr Aufwand als der Rest
dieser Iteration — User-Entscheidung, Trade-off wurde explizit besprochen.

**Konsequenz:** `lib/telemetry/bmp280.{h,cpp}` und `test/test_bmp280/`
werden **ersatzlos gelöscht**, nicht migriert.

## Komponenten

### 1. `lib/telemetry/bmp280.{h,cpp}` + `test/test_bmp280/` — löschen

Vollständig entfernen. Kein Nachfolger in `lib/telemetry/`.

### 2. `lib/telemetry/record.h` / `record.cpp` — Feld- und Spaltenumbau

- `has_bmp` → `has_bme` (ein gemeinsames Flag für alle vier Felder, wie bisher
  bei GPS/BMP: sie stammen aus demselben Sensor-Lesezyklus).
- Neues Feld `float humidity_pct = 0.0f;` (relative Luftfeuchte in %), ans
  Ende der bestehenden drei BMP-Felder angehängt.
- CSV-Spaltenreihenfolge (Stelle 2, `csv_header()`):
  `t_ms,utc,phase,fix_q,lat,lon,alt_gps_m,sats,temp_c,pressure_hpa,alt_baro_m,humidity_pct`
  (12 Spalten statt bisher 11).
- `csv_row()` (Stelle 3a): bei `has_bme` vier Werte schreiben (temp_c,
  pressure_hpa, alt_baro_m, humidity_pct), sonst vier leere Felder.
- `parse_csv_row()` (Stelle 3b): Spaltenzahl-Check von 11 auf 12; Feld [11]
  = humidity_pct lesen, wenn `has_bme` (weiterhin abgeleitet aus „temp_c
  nicht leer").
- Round-Trip-Tests wie gehabt: mit/ohne BME, leere Felder korrekt.

### 3. `lib/telemetry/display_status.h` / `.cpp` — Flag umbenennen

`DisplayState::bmp_ok` → `bme_ok`. Die sichtbare Kürzel-Anzeige auf dem OLED
bleibt **"B:ok"/"B:--"** (B steht weiter für den Luftdruck-/Klimasensor-Slot),
nur der interne Feldname ändert sich für Konsistenz mit `has_bme`.

### 4. `src/flight/pins.h` — Kommentar aktualisieren

Zeile `// --- I2C (geteilt mit OLED) : BMP280 + MPU-6050 ---` →
`// --- I2C (geteilt mit OLED) : BME280 + MPU-6050 ---`. Pins (SDA 4, SCL 15)
bleiben unverändert — gleicher I²C-Bus, andere Sensor-Bezeichnung.

### 5. `src/flight/bme_sensor.h` / `.cpp` — neuer I²C-Wrapper (Adafruit-Lib)

Dünne Schicht um `Adafruit_BME280`, analog zu `src/flight/oled.{h,cpp}`
(Hardware-Init + Lesen, keine eigene Formel):

```cpp
// bme_sensor.h — BME280 (Temperatur/Druck/Feuchte) via Adafruit-Bibliothek.
// I2C-Adresse fest 0x76 (typischer Default günstiger BME280-Breakouts).
// QNH fest 1013,25 hPa (Standardatmosphäre, keine Boden-Kalibrierung — YAGNI).
#ifndef FLIGHT_BME_SENSOR_H
#define FLIGHT_BME_SENSOR_H

#include "record.h"

// I2C muss vorher laufen (Wire.begin() bzw. über OLED/U8g2 bereits aktiv).
// Rückgabe: true, wenn der Sensor am Bus antwortet (Chip-ID ok).
bool bme_begin();

// Liest den Sensor und füllt has_bme/temp_c/pressure_hpa/alt_baro_m/
// humidity_pct in rec. Bei Lesefehler bleibt has_bme unverändert false und
// die vier Felder werden nicht angefasst.
void bme_read(telemetry::TelemetryRecord& rec);

#endif // FLIGHT_BME_SENSOR_H
```

`bme_sensor.cpp` hält ein statisches `Adafruit_BME280`-Objekt, `bme_begin()`
ruft `begin(0x76)` auf und liefert dessen Erfolg zurück; `bme_read()` prüft
intern ein „Sensor ok"-Flag (aus `bme_begin()`) und schreibt bei Erfolg
`readTemperature()`, `readPressure()/100.0f` (Pa → hPa),
`readAltitude(1013.25f)`, `readHumidity()` in den Record.

### 6. `src/flight/main.cpp` — Verdrahtung

- `#include "bme_sensor.h"` ergänzen.
- In `setup()`: `bool g_bme_ok = bme_begin();` nach dem SD-Init, vor dem
  CSV-Header-Print.
- In `loop()`, direkt vor dem Bau der CSV-Zeile (nach dem Befüllen der
  GPS-Felder in `g_rec`, vor `csv_row(g_rec)`): `bme_read(g_rec);` — der
  Sensor wird also **pro eingehender GGA-Zeile** mitgelesen (gemeinsamer
  Rhythmus mit GPS, kein eigener Timer).
- `DisplayState::bme_ok = g_bme_ok;` im bestehenden OLED-Block ergänzen.
- Die auskommentierte `>>> TEMP DEBUG (GPS-Empfangsdiagnose) <<<`-Ausgabe
  (rohe NMEA-Zeilen auf Serial) wird entfernt — sie war laut eigenem
  Kommentar nur zur Diagnose gedacht und sollte danach wieder raus.

### 7. `platformio.ini` — Abhängigkeiten

- `adafruit/Adafruit BMP280 Library` → `adafruit/Adafruit BME280 Library`.
- `adafruit/Adafruit Unified Sensor` neu ergänzen (Pflicht-Abhängigkeit der
  BME280-Lib, sonst Compile-Fehler wegen `Adafruit_Sensor.h`).

### 8. Tests — anpassen/erweitern

- `test/test_bmp280/` löschen.
- `test/test_record/test_record.cpp`: bestehende `*_bmp*`-Tests umbenennen
  (`has_bme`), Spaltenzahl-Erwartung auf 12 anpassen (`test_wrong_column_
  count_rejected` nutzt jetzt eine 8-Spalten-Zeile als „zu kurz"), neuer
  Round-Trip-Test inkl. `humidity_pct`.
- `test/test_display_status/test_display_status.cpp`: `bmp_ok` → `bme_ok` in
  allen Test-Funktionen (`test_sensors_all_ok`, `_all_missing`, `_mixed`).

### 9. Doku aktualisieren

- `CLAUDE.md`: Pin-Tabelle „BMP280 + MPU-6050" → „BME280 + MPU-6050".
- `docs/software-flow.md`: `bmp280.cpp`-Knoten entfernen (kein Ersatz-Knoten
  in `lib/`, da jetzt Adafruit-Lib in `src/flight`); `READ_SENS`-Knoten und
  Landkarten-Tabelle (§6) auf BME280 + neuen `src/flight/bme_sensor`-Baustein
  aktualisieren (Status 🔶 „geschrieben, Board-Verifikation offen", bis der
  erste echte Boden-Test gelaufen ist).
- `TODO.md`: BMP280-Erwähnungen auf BME280 korrigieren.

## Grenzen (YAGNI)

- **QNH fest 1013,25 hPa** (Standardatmosphäre). Keine Boden-Kalibrierung
  beim Start — für die Flugphasen-Erkennung reicht die *relative* Höhen-
  änderung, absolute Genauigkeit ist nicht nötig.
- **I²C-Adresse fest `0x76`**, kein Auto-Scan zwischen 0x76/0x77. Falls falsch,
  meldet `bme_begin()` das sofort am seriellen Monitor (Rückgabewert false).
- **Kein eigener Lesetakt** für den BME280 — er wird synchron zum GPS-GGA-Takt
  gelesen, kein zusätzlicher Millis()-Timer wie beim OLED-Refresh.
- **Kein natives Unit-Test-Modul** für die Sensor-Umrechnung (siehe Architektur-
  Bruch oben) — bewusste, besprochene Abweichung von der sonstigen Test-
  Strategie des Projekts.

## Testreihenfolge nach der Umsetzung (Board)

Diese Iteration liefert Code, der **am Board verifiziert** werden muss (analog
zu GPS-Flight-Mode): `bme_begin()` liefert `true`, `bme_read()` liefert
plausible Werte (Zimmertemperatur, ~1013 hPa auf Meereshöhe ± Wetter, Luft-
feuchte 30–70 % im Raum). Gehört in die bestehende TODO.md-Testreihenfolge
(„Integrationstest — alle Sensoren + Logging + Funk").

## Quellen

- Adafruit BME280 Library (GitHub: `adafruit/Adafruit_BME280_Library`) —
  `begin(0x76)`, `readTemperature()`, `readPressure()` (Pa), `readHumidity()`
  (%), `readAltitude(seaLevelhPa)`.
- Adafruit Lernseite „Adafruit BME280 Humidity + Barometric Pressure +
  Temperature Sensor Breakout" — Arduino-Testcode, I²C-Adressen 0x76/0x77.
- PlatformIO Registry: `adafruit/Adafruit BME280 Library` benötigt
  `Adafruit Unified Sensor` als Abhängigkeit.
