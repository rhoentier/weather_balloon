# CLAUDE.md — Wetterballon-Projekt

Schul-Wetterballon mit ESP32-Microcontroller: Messdaten loggen + Live-Tracking
zur Bergung. Flugzeit 2–3 h, Nutzlast < 200 g, Budget < 100 €.

Sprache im Projekt: **Deutsch** (Doku, Commits, Kommentare, Antworten an den User).
Der User ist Softwareentwickler, aber ggf. neu im Embedded-Bereich → bei
Hardware-Stolpersteinen kurz das *Warum* miterklären.

---

## Arbeitsweise (verbindlich)

### Einfachheit vor Prozess
Das Wichtigste ist ein **einfacher, verständlicher Aufbau**. Code so schreiben,
dass man ihn direkt aufs Board flashen und über `pio device monitor`
nachvollziehen kann, was passiert. Kein Overengineering, keine
Abstraktionsschichten auf Vorrat, keine Architektur auf Vorrat.

### YAGNI — strikt
Nur bauen, was *jetzt* gebraucht wird. Konkret beim Telemetrie-Record: **keine
leeren Sensor-Platzhalter** vorhalten. Ein Sensor kommt erst dann ins Format,
wenn er tatsächlich gelesen wird. Erweiterung pro Sensor = genau drei Stellen
(siehe unten). Kein „könnte man später brauchen"-Code, keine Konfig-Optionen
auf Vorrat, keine Abstraktionsschicht für hypothetische Fälle.

### Keine Tests schreiben
In diesem Projekt werden **keine automatisierten Tests** geschrieben. Neuen Code
nicht mit Tests absichern. Verifiziert wird ausschließlich am Verhalten: Code
schreiben, aufs Board flashen, am `pio device monitor` / am tatsächlichen
Verhalten prüfen, ob es tut was es soll.

### Bibliotheken bevorzugen
Für Standardaufgaben (Sensor-Treiber, Protokolle, Parsing etc.) bevorzugt
etablierte, gut gepflegte Bibliotheken einsetzen statt Funktionalität selbst
neu zu implementieren. Eigenimplementierung nur, wenn keine passende Library existiert oder sie
unnötigen Ballast/Abhängigkeiten mitbringt.

### Fakten verifizieren statt raten
Hardware-/Pin-/Protokoll-Angaben gegen belastbare Quellen prüfen (Datenblatt,
RIOT-OS-Header, offizielle Pinouts) — nicht aus dem Gedächtnis. Mehrere Quellen
haben hier schon widersprochen (siehe „Gelernte Fallen").

### Software-Flow mitpflegen
`docs/software-flow.md` beschreibt den Ablauf der Firmware (Mermaid-Diagramme +
Status-Marker ✅/🔶/⬜). Nach **jeder** Änderung am Software-Ablauf (neuer
Baustein, geänderter Übergang, Statuswechsel ⬜ → 🔶 → ✅) wird dieses Dokument
**am Ende der Arbeit** aktualisiert — Diagramme, Marker und die TODO-Landkarte.
Ein veraltetes Flow-Diagramm täuscht einen Stand vor, den der Code nicht hat.

---

## Hardware (verifiziert)

**Flug-Board: Heltec WiFi LoRa 32 V2** (ESP32 + SX1276 868 MHz + OLED).
**Bodenstation: Heltec V3** (ESP32-S3 + SX1262) — anderes Board, NICHT mischen!

### Pinbelegung Flug-Einheit (V2)
Vollständig in `docs/superpowers/specs/2026-06-27-pinbelegung-heltec-v2.md`,
als Code in `src/flight/pins.h`. Kurz:

| Funktion | GPIO | |
|---|---|---|
| GPS NEO-6M | RX **23**, TX **17** | UART2, 9600 Baud. Gekreuzt (TX↔RX). TX ist Pflicht (Flight-Mode!) |
| BMP280 + MPU-6050 | SDA **4**, SCL **15** | I²C, geteilt mit OLED |
| DS18B20 | **22** | 1-Wire, 4,7 kΩ Pull-up nach 3V3 |
| GUVA-S12SD (UV) | **36** | ADC1, input-only |
| microSD | CS **13** | SPI, Bus mit LoRa geteilt (SCK5/MISO19/MOSI27) |
| Vext / LED / Button | 21 / 25 / 0 | board-intern |

**Board-intern (NICHT anfassen):** LoRa SCK5 MISO19 MOSI27 NSS18 RST14 DIO0 26
DIO1 35 DIO2 34 | OLED SDA4 SCL15 RST16 | Vext21 | LED25 | Button0.

### Gelernte Fallen (teuer, wenn übersehen)
- **GPS-Pins ≠ Tutorial-Pins:** Viele ESP32-Tutorials nutzen GPIO16/17 für
  UART2 — **GPIO16 ist beim V2 der OLED-Reset!** Darum 23/17.
- **UV nur an ADC1** (GPIO 32–39): ADC2 ist bei aktivem Funk blockiert.
- **Board-Verwechslung:** Diagramme/Pinouts für V3 (ESP32-S3, LoRa auf GPIO8–14,
  `LoRa_BUSY` = SX1262-Fingerabdruck, OLED 17/18) gelten NICHT für das V2.
- **Stromversorgung — offener Punkt:** 5V-Pin verträgt laut Datenblatt nur
  **4,7–6,0 V**. 4× frische Energizer L91 (~1,8 V/Zelle) können **>6 V** liefern.
  → vor dem Flug messen, ggf. Step-Down auf 5,0 V. Steht in TODO-Testreihenfolge.

---

## Build & Flash

PlatformIO (Core 6.1.19). CLI liegt unter `~/.platformio/penv/bin/` — entweder
zum PATH hinzufügen oder die VSCode-PlatformIO-Buttons nutzen.

```bash
export PATH="$PATH:$HOME/.platformio/penv/bin"   # für Terminal-Nutzung
pio run  -e flight           # Flug-Firmware für ESP32 kompilieren
pio run  -e flight -t upload # aufs Board flashen
pio device monitor           # serielle Ausgabe (115200 Baud)
```

### Environments (`platformio.ini`)
- `flight` — Flug-Einheit (board `heltec_wifi_lora_32_V2`)
- `ground` — Bodenstation (board `heltec_wifi_lora_32_V3`)

---

## Projektstruktur

```
platformio.ini            # Environments
lib/telemetry/            # GETEILTE, hardware-freie Logik
  flight_phase.h/.cpp     #   Flugphasen-Automat (PreFlight/Ascent/Descent/Landed)
  ubx.h/.cpp              #   UBX-CFG-NAV5 bauen + Checksumme + ACK-Parsing
  record.h/.cpp           #   Telemetrie-/CSV-Format (SD + LoRa, gemeinsam)
src/flight/               # Flug-Firmware (Arduino, am Board verifiziert)
  pins.h, main.cpp, gps_flightmode.h/.cpp
  gps_reader.h/.cpp       #   GPS-Auslese über TinyGPSPlus (feed/fill/display_state)
src/ground/main.cpp       # Bodenstation (Platzhalter)
docs/superpowers/specs/   # Konzept + Pinbelegung
TODO.md                   # Projektschritte + Testreihenfolge (Hardware-Tests)
```

---

## Telemetrie-/CSV-Format — die zentrale Schnittstelle

EIN Format für SD-Log **und** LoRa-Telemetrie (Konzept §7.2: beide Logs hinterher
vergleichbar). Direkt in Excel/Python lesbar. Fehlende Werte = **leeres Feld**.

`lib/telemetry/record.h`:
```cpp
struct TelemetryRecord { uint32_t t_ms; Phase phase; bool has_fix;
                         double lat, lon; float alt_gps_m; uint8_t sats; };
std::string csv_header();
std::string csv_row(const TelemetryRecord&);
bool        parse_csv_row(const std::string&, TelemetryRecord&);  // Round-trip!
```

**Sensor hinzufügen = genau 3 Stellen** (im Header dokumentiert):
1. Feld in `TelemetryRecord` (mit `has_*`-Flag, falls optional)
2. Spaltenname in `csv_header()`
3. Wert in `csv_row()` schreiben **und** in `parse_csv_row()` lesen

Reihenfolge der Spalten muss in header/row/parse identisch sein — sonst passen
beim Einlesen die Spalten nicht mehr zusammen (z.B. Spaltenzahl-Mismatch).

---

## Stand & Nächstes

Fertig: Flugphasen, GPS-Flight-Mode-Logik, CSV-Format. GPS-Flight-Mode-Senden
(`src/flight`) ist **am echten NEO-6M verifiziert** — mit gekreuzter UART-
Verdrahtung (ESP32-TX 17 → GPS-RX, ESP32-RX 23 ← GPS-TX) kommt der UBX-ACK zurück.

Offene Hardware-Tests siehe `TODO.md` (GPS-Flight-Mode am Boden, microSD 3,3/5 V,
Batteriespannung, Kältetest, Reichweite, Generalprobe).
