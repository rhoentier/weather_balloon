# Software-Flow — Wetterballon Flug-Einheit

Dieses Dokument erklärt den **Ablauf der Flug-Firmware**: welche Schritte in
welcher Reihenfolge passieren, *was* dabei geschieht und *warum*. Es beschreibt
den Ist-Stand des Codes und markiert klar, was schon implementiert ist und was
noch aussteht.

**Legende in den Diagrammen:**
`✅ fertig & nativ getestet` · `🔶 geschrieben, Board-Verifikation offen` · `⬜ noch nicht implementiert`

> **⚠️ Dieses Dokument mitpflegen!**
> Dieser Flow soll den *echten* Stand widerspiegeln. Nach **jeder** Änderung am
> Software-Ablauf (neuer Baustein, geänderter Übergang, Status von ⬜ → 🔶 → ✅)
> wird dieses Dokument **am Ende der Arbeit aktualisiert** — Diagramme, Status-
> Marker und die TODO-Landkarte in §6. Ein veraltetes Flow-Diagramm ist
> schlimmer als keines: Es täuscht einen Stand vor, den der Code nicht hat.

---

## 1. Überblick — Warum diese Architektur?

Der Kern-Grundsatz (siehe `CLAUDE.md`): **testbare Logik ist hardware-frei**.
Alles, was man auf dem Mac in Millisekunden testen kann, lebt in `lib/telemetry/`
(reines C++17, kein `<Arduino.h>`). Nur das, was echte Hardware *zwingend*
braucht (UART, I²C, SPI, Funk), lebt in `src/flight/`.

```mermaid
flowchart TB
    subgraph HW["src/flight/ — braucht echte Hardware (am Board verifiziert)"]
        direction TB
        MAIN["main.cpp<br/>Setup + Loop"]
        GPSFM["gps_flightmode.cpp<br/>UBX senden + ACK lesen"]
        BMP["bmp_sensor.cpp<br/>I2C-Lesen via Adafruit-Lib (0x76)"]
        DS["ds18b20.cpp<br/>1-Wire-Lesen, async (Außentemp.)"]
    end

    subgraph LIB["lib/telemetry/ — hardware-frei (nativ getestet ✅)"]
        direction TB
        UBX["ubx.cpp<br/>UBX-Bytes bauen + Checksumme + ACK prüfen"]
        PHASE["flight_phase.cpp<br/>Flugphasen-Automat"]
        REC["record.cpp<br/>CSV-Format (bauen + parsen)"]
    end

    MAIN --> GPSFM
    GPSFM -->|nutzt| UBX
    MAIN -.->|später: aufrufen| BMP
    MAIN -.->|später: Höhe füttern| PHASE
    MAIN -.->|später: Zeile schreiben| REC

    style UBX fill:#d4edda
    style PHASE fill:#d4edda
    style REC fill:#d4edda
    style GPSFM fill:#fff3cd
    style MAIN fill:#fff3cd
    style BMP fill:#fff3cd
```

**Warum die Trennung?** Die fehleranfälligsten Teile — die exakte UBX-Byte-
Sequenz, die Landungserkennung, das CSV-Round-Trip — sind genau die, die man
am fliegenden Ballon *nicht* debuggen kann. Also werden sie vom Board entkoppelt
und gegen bekannt-korrekte Referenzen getestet.

---

## 2. Ablauf `setup()` — einmalig beim Boot

Das ist der Teil, der heute in `src/flight/main.cpp` implementiert ist. Der
kritischste Baustein steht bewusst ganz vorn: der GPS-Flight-Mode.

```mermaid
flowchart TD
    START([Board bootet]) --> SER["Serial @115200 starten<br/>(Debug-Ausgabe)"]
    SER --> UART2["Serial2 starten<br/>9600 Baud, RX23/TX17<br/>= NEO-6M an UART2"]
    UART2 --> FM{"set_gps_flight_mode()<br/>Dynamic Model 6 setzen"}

    FM -->|ACK empfangen| OK["✅ Flight-Mode bestätigt<br/>Log: BESTAETIGT"]
    FM -->|kein ACK nach Retries| FAIL["⚠️ Log: NICHT bestaetigt<br/>— prüfen!"]

    OK --> LOOP([weiter zu loop])
    FAIL --> LOOP

    style FM fill:#fff3cd
```

**Warum der Flight-Mode zwingend ist:** Der NEO-6M schaltet im Standard-Modus
oberhalb ~18 km die Positionsberechnung ab (CoCom-Grenze). Ohne „Airborne <1g"
(Modell 6) hätten wir also ausgerechnet in der interessanten Höhe **keine
Position** — und könnten den Ballon nicht mehr bergen. Deshalb wird das gleich
beim Boot gesetzt und per ACK verifiziert.

### 2a. Detail: Wie `set_gps_flight_mode()` das ACK absichert

Dieser Ablauf ist der Grund, warum die Byte-Logik separat (nativ) getestet wird —
hier steckt die meiste Fehlerquelle.

```mermaid
sequenceDiagram
    participant FW as gps_flightmode.cpp<br/>(src/flight 🔶)
    participant UBX as ubx.cpp<br/>(lib ✅)
    participant GPS as NEO-6M

    FW->>UBX: build_cfg_nav5_airborne(msg)
    UBX-->>FW: 44 Byte inkl. Fletcher-Checksumme

    loop bis zu 'retries' Versuche
        FW->>GPS: Eingangspuffer leeren (altes NMEA weg)
        FW->>GPS: msg senden (UART) + flush
        FW->>GPS: bis Timeout auf Antwort lauschen
        GPS-->>FW: Byte-Strom (NMEA + evtl. ACK)
        Note over FW: resync auf 0xB5 0x62,<br/>10-Byte-Fenster sammeln
        FW->>UBX: is_cfg_nav5_ack(buf)?
        alt gültiges ACK-ACK für CFG-NAV5
            UBX-->>FW: true
            Note over FW: return true ✅
        else kein passendes ACK
            UBX-->>FW: false
            Note over FW: weitersuchen / nächster Versuch
        end
    end
```

**Warum Puffer leeren + Resync?** Das GPS sendet permanent NMEA-Textzeilen. Das
kurze binäre ACK (beginnt mit `0xB5 0x62`) muss aus diesem Strom herausgefischt
werden — sonst „verdeckt" altes Geschwätz die Bestätigung. Der Resync-Automat
im Code sucht robust den Sync-Anfang, auch wenn mitten in einer NMEA-Zeile
begonnen wird.

---

## 3. Ablauf `loop()` — pro Messzyklus (Ziel-Architektur)

**Wichtig:** `loop()` ist inzwischen bis zur microSD verdrahtet (grün + gelb),
aber die gelben Knoten (🔶) sind **am Board noch nicht verifiziert** — nur der
Flight-Build ist geprüft (kompiliert fehlerfrei). Der folgende Ablauf zeigt, wo
die nativ getesteten Logik-Bausteine (grün) andocken, was verdrahtet, aber
board-unverifiziert ist (gelb), und was noch fehlt (weiß).

```mermaid
flowchart TD
    L([loop-Iteration]) --> READ_GPS["GPS lesen<br/>🔶 TinyGPSPlus (gps_reader: feed/fill), zeitgesteuert 1 Hz"]
    READ_GPS --> READ_SENS["Sensoren lesen (I²C/1-Wire/ADC)<br/>🔶 BMP280 (bmp_sensor) + DS18B20 (ds18b20, async) verdrahtet, Board-Test offen; ⬜ MPU-6050, UV"]

    READ_SENS --> PHASE["Flugphase aktualisieren<br/>✅ FlightPhaseDetector.update(alt, t)"]
    PHASE --> BUILD["TelemetryRecord befüllen<br/>🔶 in main.cpp verdrahtet"]

    BUILD --> CSV["CSV-Zeile bauen<br/>🔶 csv_row() verdrahtet"]

    CSV --> SD["Auf microSD schreiben<br/>🔶 sd_log() verdrahtet"]
    CSV --> LORA["Per LoRa senden<br/>⬜ SX1276, board-intern"]

    SD --> RATE{"Phase == Landed?<br/>✅ Automat liefert Phase"}
    LORA --> RATE
    RATE -->|ja| SLOW["Bergungsmodus:<br/>⬜ Spar-Logging + GPS-Bake"]
    RATE -->|nein| FAST["normale Rate<br/>weiter"]

    SLOW --> OLED
    FAST --> OLED

    OLED{"OLED aktiv &&<br/>alle 500 ms?<br/>🔶 am Ende der loop-Iteration,<br/>UNABHÄNGIG vom GPS-Empfang"}
    OLED -->|nein| L
    OLED -->|ja, noch PreFlight| SHOW["status_lines() aus g_rec/g_detector<br/>+ oled_show()<br/>✅ Formatierung nativ getestet<br/>🔶 Zeichnen am Board offen"]
    OLED -->|ja, Phase-Wechsel| OFF["oled_off() — einmalig<br/>🔶 Vext-Abschaltung Board-Test offen"]
    SHOW --> L
    OFF --> L

    style PHASE fill:#d4edda
    style BUILD fill:#fff3cd
    style CSV fill:#fff3cd
    style RATE fill:#d4edda
    style READ_GPS fill:#fff3cd
    style READ_SENS fill:#fff3cd
    style SD fill:#fff3cd
    style LORA fill:#f8f9fa
    style SLOW fill:#f8f9fa
    style OLED fill:#fff3cd
    style SHOW fill:#fff3cd
    style OFF fill:#fff3cd
```

**OLED-Boden-Check (`status_lines()` + `oled_*`):** Der Display-Block läuft
**am Ende jeder `loop()`-Iteration**, bewusst **entkoppelt vom GPS-Empfang** —
also auch dann, wenn gerade keine GPS-Daten ankommen. Das ist der Grund für die
Entkopplung: Ein *stummes* GPS-Modul (nicht verkabelt, defekt) muss als
`GPS: --` sichtbar werden — läge das Zeichnen hinter dem GPS-Einlesen, bliebe
das Display genau in diesem Fehlerfall leer. Gedrosselt wird auf höchstens
einen Redraw alle 500 ms (`OLED_REFRESH_MS`), damit nicht jede Iteration aufs
I²C schreibt. Der Zustand kommt aus dem persistenten `g_rec`/`g_detector`
(nicht aus einem lokalen Fix). Solange die Phase `PreFlight` ist, zeigt das
Display fünf Zeilen (Titel, GPS-Stufe inkl. Satellitenzahl, SD-Status, Phase,
Sensorstatus) — der „Startklar?"-Check ohne Laptop am Startort. Die fünfte
Zeile fasst die vier Sensoren als Kurzcodes zusammen (`B:ok M:-- D:-- U:--` für
BMP280/MPU-6050/DS18B20/UV) — `ok`, wenn das jeweilige `*_ok`-Flag im
`DisplayState` gesetzt ist, sonst `--`. `src/flight/main.cpp` befüllt inzwischen
`bmp_ok` aus `bmp_begin()` und `ds18b20_ok` aus `ds_begin()`; `mpu_ok`/`uv_ok`
bleiben noch **unbefüllt** (⬜), bis die jeweilige Sensor-Anbindung steht. Beim ersten Übergang in
`Ascent` wird `oled_off()` über das `g_oled_active`-Flag genau einmal
aufgerufen (Vext + Display aus): am fliegenden Ballon ist ein aktives Display
unnötiger Stromverbrauch. Die Zeilen-Formatierung
(`lib/telemetry/display_status`) ist nativ getestet (✅); das tatsächliche
Zeichnen und die Vext-Abschaltung am echten SSD1306 sind noch **nicht am Board
verifiziert** (🔶) — Teil der TODO-Testreihenfolge.

**Warum EIN gemeinsames CSV-Format für SD *und* LoRa?** So sind das
Bord-Log (vollständig, auf SD) und das Empfangs-Log (lückenhaft, per Funk)
hinterher direkt Zeile für Zeile vergleichbar — man sieht z. B., welche
Telemetrie-Pakete unterwegs verloren gingen (Konzept §7.2).

---

## 4. Flugphasen-Automat — das sicherheitskritische Herzstück

`flight_phase.cpp` bekommt nur **Höhe + Zeit** und entscheidet daraus die Phase.
Bewusst als reiner Zustandsautomat gebaut, damit „Landung erkennen" ohne echten
Flug testbar ist.

```mermaid
stateDiagram-v2
    [*] --> PreFlight

    PreFlight --> Ascent: Steigrate ≥ 1 m/s<br/>UND ≥ 5 s anhaltend<br/>(entprellt GPS-Rauschen)
    Ascent --> Descent: Sinkrate ≤ -3 m/s<br/>(= Ballon geplatzt / Burst)
    Descent --> Landed: wieder niedrig (< 800 m)<br/>UND ≥ 60 s stabil (|v| < 0.5 m/s)
    Landed --> [*]

    note right of Descent
        Kein Rückweg nach Ascent.
        Einmal am Sinken bleibt es
        Sinken — GPS-Rauschen darf
        die Phase nicht zurückwerfen.
    end note

    note right of Landed
        Endzustand → Bergungsmodus:
        Strom sparen, GPS-Bake senden.
    end note
```

**Warum Entprellen (die Zeitschwellen)?** Am Boden zappelt die GPS-Höhe um
mehrere Meter. Ein einzelner „Sprung" nach oben darf nicht sofort „Aufstieg"
auslösen (sonst startet das volle Logging zu früh), und ein einzelner Ausreißer
nach unten darf nicht „Landung" melden (sonst geht die Firmware fälschlich in
den Sparmodus, während der Ballon noch fliegt). Deshalb müssen die Bedingungen
über eine Mindestzeit *anhalten*.

**Warum keine Rücksprünge?** Die Übergänge sind bewusst als Einbahnstraße
modelliert (PreFlight → Ascent → Descent → Landed). Das macht das Verhalten
vorhersehbar und robust gegen Messrauschen — der teuerste Fehler wäre, den
Bergungsmodus zu verpassen, weil die Phase hin- und herspringt.

---

## 5. CSV-Record — die zentrale Datenschnittstelle

`record.cpp` baut aus einem `TelemetryRecord` eine CSV-Zeile und kann sie wieder
zurücklesen (`parse_csv_row`). Der **Round-Trip** (schreiben → parsen → gleicher
Record) ist die Testabsicherung.

```mermaid
flowchart LR
    R["TelemetryRecord<br/>{t_ms, utc, phase, fix_q, has_fix,<br/>lat, lon, alt, sats, BMP...}"]
    R -->|csv_row| LINE["CSV-Zeile<br/>'12345,12:35:19,ASCENT,1,48.1,11.5,530.0,7,...'"]
    LINE -->|parse_csv_row| R2["TelemetryRecord<br/>(identisch)"]

    R2 -.->|"Round-Trip-Test ✅<br/>muss == R sein"| R
```

**Warum leere Felder statt Nullwerte?** Fehlt der GPS-Fix, werden lat/lon/alt/sats
als **leere** CSV-Spalten geschrieben (`12345,,ASCENT,,,,`), nicht als `0`.
Grund: `0.0` wäre eine gültige Koordinate (Golf von Guinea) — ein leeres Feld ist
eindeutig „kein Wert" und wird in Excel/Python korrekt als NaN gelesen.

**Erweitern nach YAGNI (aus dem Header):** Ein neuer Sensor = genau drei Stellen —
Feld im Record, Spaltenname in `csv_header()`, Wert in `csv_row()` + Lesen in
`parse_csv_row()`. Die Spaltenreihenfolge muss in allen dreien identisch sein;
der Round-Trip-Test fängt jede Abweichung sofort.

**Warum eine eigene `fix_q`-Spalte?** Sie trägt die **rohe** GGA-Fix-Qualität
(`0`=kein Fix, `1`=GPS, `2`=DGPS …) und steht direkt nach `phase`. `gps_fill()`
übernimmt sie aus `TinyGPSPlus` (`gps.location.FixQuality()`) und leitet daraus
`has_fix = (fix_q > 0)` ab, aber für die Diagnose am Boden ist der Unterschied
zwischen „nie ein Fix" und „Fix verloren" wertvoll. Anders als die GPS-Positionsfelder wird `fix_q` **immer**
geschrieben (auch `0`), denn `0` ist hier ein echter Messwert, kein fehlender.

**Warum eine eigene `utc`-Spalte?** Sie trägt die absolute GPS-Uhrzeit als
`hh:mm:ss` (aus Feld [1] der GGA-Zeile) und steht bewusst **entkoppelt von
`has_fix`** — das GPS liefert oft schon eine gültige Uhrzeit, bevor ein
Positions-Fix steht; fehlt sie, bleibt das Feld leer (`has_utc=false`). Neben
dem monotonen, aber bei Neustart zurückspringenden `t_ms` steht damit eine
absolute, neustartfeste Zeit zur Verfügung — das macht einen `t_ms`-Rücksprung
harmlos und erlaubt den Abgleich von SD-Log, Empfangs-Log und Action-Cam über
die Uhrzeit.

**Warum ein gemeinsames `has_bmp`-Flag für drei Spalten?** `temp_c`,
`pressure_hpa` und `alt_baro_m` stammen aus demselben BMP280-Lesezyklus — fehlt
der Sensor (nicht bestückt, I²C-Fehler), fehlen zwangsläufig alle drei, also
genügt ein gemeinsames Flag statt dreier einzelner. Die Umrechnung übernimmt
bewusst **nicht** `lib/telemetry`, sondern die Adafruit-BMP280-Bibliothek direkt
in `src/flight/bmp_sensor` — eine bewusste Abweichung von der sonstigen
Hardware-frei-Regel (der verbaute Chip ist ein BMP280, Chip-ID 0x58 am Board
bestätigt — KEINE Luftfeuchte; siehe `docs/superpowers/specs/2026-07-05-bme280-umbau-design.md`).

**Warum ein zweiter Temperatursensor (DS18B20) neben dem BMP280?** Der BMP280
sitzt im Modul und misst dessen Eigenwärme mit; der DS18B20 hängt am Kabel außen
und liefert die echte **Außentemperatur** (Spalte `temp_ext_c`, eigenes
`has_ds`-Flag). Die Anbindung liegt — wie beim BMP280 — bewusst in `src/flight`
(`ds18b20`), weil sie die DallasTemperature-Lib direkt nutzt. Gelesen wird
**non-blocking**: eine 12-Bit-Wandlung dauert bis zu ~750 ms, was den GPS-UART-
Loop blockieren würde. `ds_update()` stößt daher nur eine Wandlung an, holt den
fertigen Wert beim nächsten Durchlauf ab und startet die nächste.

**Warum vier einzelne `*_ok`-Flags in `DisplayState` statt eines CSV-Felds?**
Der Sensorstatus in der OLED-Zeile ist reine **Boden-Diagnose** („ist der
Sensor gerade ansprechbar?"), keine Telemetrie fürs CSV-Log — er gehört daher
zu `lib/telemetry/display_status`, nicht zu `record`. Vier einzelne `bool`s
(analog zu `sd_ok`) statt eines generischen Arrays, weil es bei genau vier
fest benannten Sensoren bleibt (YAGNI).

---

## 6. Was fehlt noch? (Landkarte zum TODO)

| Baustein | Ort | Status |
|---|---|---|
| GPS-Flight-Mode Byte-Logik | `lib/telemetry/ubx` | ✅ nativ getestet |
| GPS-Flight-Mode senden/ACK | `src/flight/gps_flightmode` | 🔶 geschrieben, Board-Test offen |
| Flugphasen-Automat | `lib/telemetry/flight_phase` | ✅ nativ getestet |
| CSV-Record (bauen/parsen) | `lib/telemetry/record` | ✅ nativ getestet |
| GPS-UTC-Spalte (hh:mm:ss) | `record` (Wert aus `gps_reader`) | ✅ CSV nativ getestet |
| NMEA-Parsing (GPS-Rohdaten → Record) | `src/flight/gps_reader` (TinyGPSPlus) | 🔶 geschrieben, Board-Test offen |
| GPS-Pipeline in loop() (1 Hz) | `src/flight/main.cpp` + `gps_reader` | 🔶 geschrieben, Board-Test offen |
| BMP280-CSV-Spalten (temp_c, pressure_hpa, alt_baro_m) | `lib/telemetry/record` | 🔶 umgesetzt |
| BMP280-I²C-Anbindung (Adafruit-Lib, in loop() verdrahtet) | `src/flight/bmp_sensor` + `main.cpp` | 🔶 geschrieben, Board-Test offen |
| DS18B20-CSV-Spalte (temp_ext_c) | `lib/telemetry/record` | 🔶 umgesetzt |
| DS18B20-1-Wire-Anbindung (async, in loop() verdrahtet) | `src/flight/ds18b20` + `main.cpp` | 🔶 geschrieben, Board-Test offen |
| Sensor-Lesung MPU-6050, UV | `src/flight/` + Umrechnung in `lib/` | ⬜ |
| microSD-Logging | `src/flight/sd_log` | 🔶 geschrieben, Board-Test offen |
| OLED-Boden-Check: Zeilen-Formatierung (inkl. Sensorstatuszeile) | `lib/telemetry/display_status` | ✅ nativ getestet |
| OLED-Sensorstatus: main.cpp befüllt `*_ok`-Flags | `src/flight/main.cpp` | ⬜ |
| OLED-Boden-Check: Zeichnen/Abschalten + Integration | `src/flight/oled` + `main.cpp` | 🔶 geschrieben, Board-Test offen |
| LoRa-Telemetrie | `src/flight/` | ⬜ |
| Bergungsmodus | `src/flight/` | ⬜ |
| Watchdog / Robustheit | `src/flight/` | ⬜ |
| Bodenstation | `src/ground/` | ⬜ Platzhalter |

Das **NMEA-Parsing** übernimmt jetzt die Bibliothek **TinyGPSPlus**
(`mikalhart/TinyGPSPlus`), gekapselt im Wrapper `src/flight/gps_reader`. Der
frühere Eigenbau (`lib/telemetry/gga` + `line_assembler`) wurde entfernt: Der
Grund für ihn — hardware-freie, nativ testbare Logik — ist entfallen, seit im
Projekt keine automatisierten Tests mehr geschrieben werden. `gps_reader` bietet
drei Funktionen: `gps_feed()` (UART-Bytes an TinyGPSPlus füttern), `gps_fill()`
(aktuellen Zustand → `TelemetryRecord`: lat, lon, alt, sats, fix_quality, UTC)
und `gps_display_state()` (Silent/Waiting/Fix fürs OLED). Siehe
`docs/superpowers/specs/2026-07-05-tinygpsplus-umstellung-design.md`.

Darauf aufbauend ist die **GPS-Pipeline in `loop()`** (`src/flight/main.cpp`)
**zeitgesteuert** verdrahtet: `gps_feed(Serial2)` füttert jede Iteration den
UART-Strom; einmal pro Sekunde (`LOG_INTERVAL_MS = 1000`) baut ein Zeit-Block
aus dem aktuellen Zustand eine Zeile: `gps_fill()` → `FlightPhaseDetector` →
`csv_row()` → Ausgabe auf Serial **und** Schreiben auf microSD über
`src/flight/sd_log` (feste Datei `/flight.csv`, Append, Header nur einmal). Der
Flight-Build kompiliert fehlerfrei — das Laufzeitverhalten ist aber **noch nicht
am echten Board verifiziert** (🔶). Das ist der erste Punkt der
TODO-Testreihenfolge: GPS-Flight-Mode am Boden prüfen und dabei die ~1-Hz-CSV-
Ausgabe über Serial sowie den Inhalt von `/flight.csv` kontrollieren.

Die **GPS-UTC-Spalte** (Zeitstempel aus dem GPS statt nur `millis()`) liefert
`gps_fill()` aus `gps.time` (`has_utc`, `utc_h/utc_min/utc_s`), entkoppelt vom
Positions-Fix; `record.cpp` schreibt sie als eigene CSV-Spalte `utc` im Format
`hh:mm:ss` direkt nach `t_ms` und liest sie im Round-Trip wieder zurück.

Der verbaute Umwelt-Sensor ist ein **BMP280** (Temperatur + Luftdruck, KEINE
Luftfeuchte). Das war zwischenzeitlich als BME280 angenommen und umgebaut worden
— am Board zeigte sich dann per I²C-Scan + Chip-ID (Register 0xD0 = **0x58** =
BMP280; BME280 wäre 0x60), dass das als „BME280" gekaufte Modul tatsächlich einen
BMP280 trägt. Der BME280-Umbau wurde daraufhin zurückgebaut. Aktueller Stand:
`TelemetryRecord` trägt `has_bmp` + die drei Spalten
`temp_c,pressure_hpa,alt_baro_m` (11 CSV-Spalten). Die Sensor-Umrechnung wird
**bewusst nicht** hardware-frei nachgebaut, sondern über die fertige
Adafruit-BMP280-Bibliothek gelesen (`src/flight/bmp_sensor`, I2C-Adresse fest
`0x76` — am Board bestätigt, der Adafruit-Default wäre 0x77 — QNH fest
1013,25 hPa). `main.cpp` ruft `bmp_begin()` in `setup()` und `bmp_read()` im
1-Hz-Log-Block der `loop()` auf; der Flight-Build kompiliert fehlerfrei, das
Laufzeitverhalten (BMP280 gefunden + plausible Werte) ist aber **noch nicht am
echten Board verifiziert** (🔶) — nächster Punkt der TODO-Testreihenfolge.

Der **OLED-Boden-Check** zeigt zusätzlich eine **Sensorstatuszeile**
(`lib/telemetry/display_status`): `DisplayState` trägt vier unabhängige
`bool`-Flags (`bmp_ok`, `mpu_ok`, `ds18b20_ok`, `uv_ok`), `status_lines()` fasst
sie als fünfte Zeile in Kurzcodes zusammen (`B:ok M:-- D:-- U:--`) — siehe
`docs/superpowers/specs/2026-07-04-oled-sensorstatus-design.md`. `bmp_ok` wird
in `src/flight/main.cpp` aus dem Rückgabewert von `bmp_begin()` befüllt;
`mpu_ok`/`ds18b20_ok`/`uv_ok` bleiben fest `false`, bis die jeweiligen Sensoren
überhaupt gelesen werden (siehe §6, MPU-6050/DS18B20/UV weiterhin ⬜).
