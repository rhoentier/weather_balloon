# Design: GPS-Auslese auf TinyGPSPlus umstellen

**Datum:** 2026-07-05
**Status:** Freigegeben, bereit für Implementierungsplan

## Ziel & Motivation

Der selbstgebaute NMEA-Parser (`lib/telemetry/gga.*` + `lib/telemetry/line_assembler.*`)
wird durch die etablierte Bibliothek **TinyGPSPlus** (`mikalhart/TinyGPSPlus`) ersetzt.

Der ursprüngliche Grund für den Eigenbau — hardware-freie, **nativ testbare**
Logik — ist entfallen, seit im Projekt keine automatisierten Tests mehr
geschrieben werden (siehe CLAUDE.md). Damit greift stattdessen die Regel
„Bibliotheken bevorzugen": weniger eigener Code, keine NMEA-Sonderfälle mehr
selbst pflegen, einfacherer Aufbau.

## Was wegfällt

- `lib/telemetry/gga.h` + `lib/telemetry/gga.cpp` — **gelöscht**
- `lib/telemetry/line_assembler.h` + `lib/telemetry/line_assembler.cpp` — **gelöscht**

TinyGPSPlus übernimmt beide Aufgaben zugleich: den UART-Byte-Strom einsammeln
**und** die NMEA-Sätze parsen.

## Was neu entsteht

TinyGPSPlus ist **Arduino-gebunden** und darf daher NICHT in das bewusst
hardware-freie `lib/telemetry`. Der GPS-Zugriff zieht in einen dünnen Wrapper in
`src/flight` (Arduino-Welt):

**`src/flight/gps_reader.h` / `gps_reader.cpp`**

- Hält das `TinyGPSPlus`-Objekt gekapselt (Datei-intern, nicht global in main.cpp).
- `void gps_feed(Stream& gps)` — liest alle verfügbaren UART-Bytes und füttert
  sie einzeln an `gps.encode(c)`.
- `void gps_fill(TelemetryRecord& r)` — schreibt den aktuellen GPS-Zustand in
  den Record (siehe Feld-Mapping).
- `GpsDisp gps_display_state()` — liefert `Silent` / `Waiting` / `Fix` fürs OLED.

### Feld-Mapping TinyGPSPlus → TelemetryRecord

| Record-Feld       | TinyGPSPlus-Quelle                              |
|-------------------|-------------------------------------------------|
| `lat` / `lon`     | `gps.location.lat()` / `gps.location.lng()`     |
| `alt_gps_m`       | `gps.altitude.meters()`                         |
| `sats`            | `gps.satellites.value()`                        |
| `fix_quality`     | `gps.location.FixQuality()`                     |
| `has_fix`         | `gps.location.isValid() && FixQuality() > 0`    |
| `has_utc`         | `gps.time.isValid()`                            |
| `utc_h/min/s`     | `gps.time.hour()/minute()/second()`             |

Das CSV-Format (`TelemetryRecord`, `csv_header`, `csv_row`, `parse_csv_row`)
bleibt damit **unverändert** — keine Spalte ändert sich.

## loop()-Umbau in `main.cpp`

Kern der Änderung: vom **satzgetriebenen** (eine CSV-Zeile pro GGA-Satz) zum
**zeitgetriebenen** Logging (eine Zeile pro Sekunde aus dem aktuellen Zustand).
Das passt besser zu einem Datenlogger und entkoppelt den Schreibtakt vom
NMEA-Satztyp.

```cpp
void loop() {
    gps_feed(Serial2);   // laufend alle UART-Bytes an TinyGPSPlus füttern

    // Zeitgesteuert: einmal pro Sekunde eine CSV-Zeile aus dem aktuellen Zustand
    if (millis() - g_last_log_ms >= LOG_INTERVAL_MS) {
        g_last_log_ms = millis();

        g_rec.t_ms = millis();
        gps_fill(g_rec);   // GPS-Felder aus TinyGPSPlus

        if (g_rec.has_fix)
            g_rec.phase = g_detector.update(g_rec.alt_gps_m, g_rec.t_ms);
        else
            g_rec.phase = g_detector.phase();  // ohne Fix letzte Phase halten

        bme_read(g_rec);

        String csv = csv_row(g_rec).c_str();
        Serial.println(csv);
        sd_log(csv);
    }

    // OLED-Block (Boden-Check) — Struktur unverändert, nur GPS-Zustandsquelle neu
    ...
}
```

- `LOG_INTERVAL_MS = 1000` (1 s) — bildet den heutigen 1-Hz-Takt des NEO-6M ab.
- Der bisherige `while (Serial2.available())`-Block mit `g_asm.push()` /
  `parse_gga()` entfällt komplett.
- `g_asm` (LineAssembler) entfällt.
- `g_gps_seen` entfällt — der Silent/Waiting/Fix-Zustand kommt jetzt aus
  `gps_display_state()`.

### Flight-Mode bleibt unberührt

`set_gps_flight_mode()` in `setup()` sendet UBX-CFG-NAV5 **vor** der
Logging-Schleife und wartet dort selbst auf das ACK. Das nutzt weiterhin
`lib/telemetry/ubx` und hat mit dem NMEA-Parser nichts zu tun. TinyGPSPlus
ignoriert die eingehenden UBX-Bytes (kein gültiges NMEA) einfach.

## OLED — Silent / Waiting / Fix

`gps_display_state()` ersetzt die heutige `g_gps_seen`-Logik:

```cpp
GpsDisp gps_display_state() {
    if (gps.charsProcessed() == 0) return GpsDisp::Silent;   // Modul stumm
    if (gps.location.isValid() && gps.location.FixQuality() > 0)
        return GpsDisp::Fix;
    return GpsDisp::Waiting;                                   // Bytes da, kein Fix
}
```

Im OLED-Block von `loop()` wird statt der
`g_rec.has_fix ? … : g_gps_seen ? …`-Kette einfach `ds.gps = gps_display_state()`
gesetzt. `ds.sats` kommt weiter aus `g_rec.sats`. Der Rest des Displays (SD,
BME, Phase) bleibt unverändert.

## platformio.ini

`mikalhart/TinyGPSPlus` steht bereits in `esp32_base.lib_deps` — **keine
Änderung nötig**. (Die Lib wird dadurch auch ins `ground`-Env gelinkt, wo sie
ungenutzt ist; das kostet nur minimal Flash. Ein Aufräumen wäre ein separater
Schritt und gehört nicht in diese Umstellung.)

## Bewusst NICHT angefasst (YAGNI)

- `TelemetryRecord` / CSV-Format — identisch, keine Spalte ändert sich.
- `lib/telemetry/ubx`, `flight_phase`, `record`, `display_status` — unberührt.
- `src/flight/gps_flightmode.*` — unberührt.
- `src/flight/bme_sensor`, `sd_log`, `oled` — unberührt.

## Doku-Nachzug (am Ende der Arbeit)

- `docs/software-flow.md` — GPS-Baustein von „line_assembler → gga" auf
  „TinyGPSPlus" umzeichnen; Status-Marker prüfen.
- `docs/superpowers/plans/2026-07-04-nmea-gga-parser.md` — als überholt/ersetzt
  markieren.
- `CLAUDE.md` — Projektstruktur: `gga` und `line_assembler` aus der
  `lib/telemetry`-Auflistung entfernen.
