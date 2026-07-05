# Design: BMP280-Umrechnung (`lib/telemetry/bmp280`)

Datum: 2026-07-04
Status: abgesegnet, bereit für Implementierungsplan

## Zweck & Grenze

Ein **hardware-freier** Logik-Baustein, der die BMP280-Rohwerte + Kalibrier-
konstanten in physikalische Größen umrechnet: **Temperatur (°C)**, **Druck (Pa)**
und daraus die **barometrische Höhe (m)**. Er schließt im `loop()`-Ablauf den
Knoten „Sensoren lesen" für den ersten Sensor.

Das Modul kennt **nur den BMP280**, nicht das Telemetrie-/CSV-Format und **kein
I²C**. Verantwortlichkeiten sauber getrennt (analog zu `gga`):

- `src/flight` liest per I²C einmalig die Kalibrierkonstanten (Register
  0x88–0x9F) und pro Zyklus die 20-bit-Rohwerte `adc_T`, `adc_P` aus, ruft die
  Umrechnungsfunktionen und kopiert die Ergebnisse in den `TelemetryRecord`.
- `lib/telemetry/bmp280` ist reine Arithmetik → nativer Unit-Test auf dem Mac in
  Millisekunden (reines C++17, kein `<Arduino.h>`).

**Faustregel-Konformität (CLAUDE.md):** Die fehleranfällige Bosch-Kompensation
(Vorzeichen der Kalibrierkonstanten, Bit-Shifts, Q-Formate) ist genau die Sorte
Logik, die am fliegenden Ballon *nicht* debuggbar ist → sie wird vom Board
entkoppelt und gegen eine **verifizierte Referenz** getestet.

## Warum diese Umrechnung nicht am Board?

Der BMP280 liefert selbst nur **Roh-ADC-Werte** plus einen Satz **Kalibrier-
konstanten** (`dig_T1..T3`, `dig_P1..P9`), die einmalig aus seinen Registern
gelesen werden. Die eigentliche Umrechnung ist Boschs fixe **Kompensations-
formel** — reine Ganzzahl-Arithmetik. Sie hängt in keiner Weise von der Hardware
ab und lässt sich damit vollständig hardware-frei testen.

## Interface (`lib/telemetry/bmp280.h`)

```cpp
#include <cstdint>

namespace telemetry {

// Die Kalibrierkonstanten des BMP280 (Register 0x88..0x9F). Der src/flight-
// Aufrufer liest sie EINMALIG per I²C aus und füllt dieses Struct. Typen exakt
// nach Datenblatt (BST-BMP280-DS001 Rev 1.26, 3.11.2) — Vorzeichen entscheidend!
struct Bmp280Calib {
    uint16_t dig_T1;                 // unsigned short
    int16_t  dig_T2, dig_T3;         // signed short
    uint16_t dig_P1;                 // unsigned short
    int16_t  dig_P2, dig_P3, dig_P4, dig_P5,
             dig_P6, dig_P7, dig_P8, dig_P9;  // signed short
};

// Bosch-Integer-Kompensation, 32 bit (Datenblatt 3.11.3).
// adc_T/adc_P sind die 20-bit-Rohwerte (positiv, in 32-bit-signed gelagert).
//
// t_fine ist die Temperatur-Zwischengröße, die die Druckformel BRAUCHT. Sie wird
// bewusst als Referenz-Parameter herausgereicht (im Datenblatt eine globale
// Variable) → die Funktionen bleiben rein, ohne verstecktes Gedächtnis.
// REIHENFOLGE: erst compensate_temperature (füllt t_fine), dann pressure.
int32_t  bmp280_compensate_temperature(int32_t adc_T,
                                        const Bmp280Calib& c,
                                        int32_t& t_fine);   // → 0,01 °C (2508 = 25,08 °C)

uint32_t bmp280_compensate_pressure(int32_t adc_P,
                                    const Bmp280Calib& c,
                                    int32_t t_fine);        // → Pa direkt (100656 = 1006,56 hPa)

// Barometrische Höhe aus Druck (internationale Höhenformel).
// sea_level_pa = Referenzdruck (QNH) auf Meereshöhe; der Aufrufer entscheidet
// die Quelle (Startdruck kalibrieren oder 101325 Pa Standardatmosphäre).
float bmp280_altitude_m(float pressure_pa, float sea_level_pa);

} // namespace telemetry
```

### Bewusste Design-Entscheidungen

1. **`t_fine` als expliziter Parameter** statt globaler Variable (wie im Daten-
   blatt-Code). Grund: reine, seiteneffektfreie Funktionen sind ohne verstecktes
   Gedächtnis testbar und in `loop()` gefahrlos wiederverwendbar.

2. **Rückgaben in nativen Einheiten des Datenblatt-Codes** (0,01 °C bzw. ganze
   Pa), *nicht* in „schönen" Einheiten. So testen wir **exakt** gegen die
   Referenzwerte. Die Umrechnung in °C/hPa für den Record macht der Aufrufer.

3. **32-bit-Variante `_int32` (Rückgabe direkt in Pa)**, nicht die 64-bit-
   Q24.8-Variante. Begründung: ~1 Pa Genauigkeit reicht für die Höhe locker
   (≈ 8 cm), und die 32-bit-Formel braucht keine 64-bit-Zwischenwerte. YAGNI:
   keine zweite (double-)Variante.

## Testreferenz — verifiziert, nicht erinnert

Das Bosch-Datenblatt (Rev. 1.26) enthält **kein** vollständig gerechnetes
Ein→Ausgabe-Beispiel (nur Ausgabe-Beispiele wie „96386 Pa"). Die Referenzwerte
wurden daher **selbst erzeugt und gegenseitig bestätigt**:

- Der **exakte 32-bit-C-Code aus dem Datenblatt (3.11.3)** wurde bit-genau
  portiert und mit dem in mehreren Bibliotheken zitierten klassischen
  Kalibriersatz durchgerechnet.
- Das Ergebnis `T = 25,08 °C` **deckt sich** mit dem in diesen Bibliotheken
  (z. B. mahfuz195/BMP280-Arduino-Library) genannten Temperaturwert → Formel
  und Community-Konstanten bestätigen sich gegenseitig.

**Kalibriersatz (Testkonstanten):**

| Konstante | Wert | | Konstante | Wert |
|---|---|---|---|---|
| dig_T1 | 27504 | | dig_P4 | 2855 |
| dig_T2 | 26435 | | dig_P5 | 140 |
| dig_T3 | -1000 | | dig_P6 | -7 |
| dig_P1 | 36477 | | dig_P7 | 15500 |
| dig_P2 | -10685 | | dig_P8 | -14600 |
| dig_P3 | 3024 | | dig_P9 | 6000 |

**Referenz-Ergebnisse (Orakel für die Tests):**

| Eingang | Zwischenwert | Ausgabe |
|---|---|---|
| `adc_T = 519888` | `t_fine = 128422` | `T = 2508` (= 25,08 °C) |
| `adc_P = 415148` | (nutzt `t_fine`) | `P = 100656` Pa (= 1006,56 hPa) |

> Diese Zahlen sind im Test als feste Konstanten hinterlegt. Die Implementierung
> wird **eigenständig** geschrieben und gegen diese externen Werte geprüft — sie
> darf sie nicht durch Kopieren derselben Formel „erschummeln".

## Barometrische Höhe

Internationale Höhenformel (barometrische Höhenformel der Standardatmosphäre):

```
h = 44330 * (1 - (p / p0) ^ (1/5.255))
```

mit `p` = gemessener Druck (Pa), `p0` = `sea_level_pa` (QNH, Pa). Eckpunkte für
die Tests:

- `p == p0` → `h ≈ 0 m` (exakt 0 bei Gleichheit).
- Ein Standardpunkt gegen die Formel, Toleranz ±1 m (Fließkomma).

Der genaue Standardpunkt wird beim Schreiben des Tests aus der Formel berechnet
(nicht aus dem Gedächtnis) und als Konstante fixiert.

## CSV-Integration (die 3-Stellen-Erweiterung)

Nach der Regel aus `record.h` — genau drei Stellen, gleiche Spaltenreihenfolge in
allen dreien:

### Stelle 1 — `TelemetryRecord` (record.h)

```cpp
// --- BMP280 (Temperatur/Druck/Barometer-Höhe) ---
bool  has_bmp = false;      // false → alle drei Felder als leere CSV-Spalten
float temp_c = 0.0f;        // °C
float pressure_hpa = 0.0f;  // hPa
float alt_baro_m = 0.0f;    // barometrische Höhe (m), aus Druck + QNH
```

Ein **gemeinsames** `has_bmp`-Flag für alle drei Felder, weil sie aus demselben
Sensor-Lesezyklus stammen: Fehlt der Sensor (nicht bestückt / I²C-Fehler),
fehlen alle drei. Fehlender Wert = **leeres Feld**, nicht `0` (Konvention wie bei
GPS: `0.0` wäre ein gültiger Messwert).

Der Record speichert die **menschenlesbaren** Größen (°C, hPa, m), nicht die
Bosch-Rohgrößen — er ist die Auswertungs-Schnittstelle für Excel/Python
(Konzept §7.2). Die Umrechnung Bosch-Einheit → °C/hPa und der Aufruf von
`bmp280_altitude_m()` passieren im `src/flight`-Aufrufer.

### Stelle 2 — `csv_header()`

Spalten `temp_c,pressure_hpa,alt_baro_m` **nach** den bestehenden GPS-Spalten
anhängen.

### Stelle 3 — `csv_row()` + `parse_csv_row()`

Werte in identischer Reihenfolge schreiben bzw. lesen; bei `!has_bmp` drei leere
Felder. Der **Round-Trip-Test** (`Record → csv_row → parse_csv_row → identisch`)
sichert die Konsistenz wie bei den bestehenden Feldern.

## Grenzen (YAGNI / Hardware-Trennung)

- **Kein I²C, kein Registerlesen, kein `<Arduino.h>`** in `lib/`. Das rohe Lesen
  der Register + Kalibrierdaten lebt später in `src/flight` und wird **am Board**
  verifiziert — nicht Teil dieser Iteration.
- **Kein Oversampling-/Modus-Setup** (Forced/Normal, `ctrl_meas`, `config`) —
  das ist Board-/Treiber-Sache.
- **Nur die 32-bit-Integer-Variante** (keine double-, keine int64-Q24.8-Variante).
- **Nur BMP280.** MPU-6050, DS18B20, UV sind eigene spätere Iterationen.

## Testreihenfolge (TDD, jeder Test erst RED sehen)

Eigener Ordner `test/test_bmp280/` mit eigenem `main()` (`UNITY_BEGIN/END`), wie
die anderen Test-Bausteine.

1. **Temperatur-Kompensation** — `adc_T=519888` → `t_fine=128422`, Rückgabe
   `2508`.
2. **Vorzeichen-Falle** — Test mit den negativen Kalibrierkonstanten
   (`dig_T3=-1000` etc.); schlägt fehl, falls signed/unsigned vertauscht wird.
3. **Druck-Kompensation** — `adc_P=415148` (mit `t_fine` aus Schritt 1) →
   `100656` Pa.
4. **Höhe** — `p==p0 → 0 m`; ein Standardpunkt mit ±1 m Toleranz.
5. **CSV-Round-Trip** — Record mit `has_bmp=true` → `csv_row` → `parse_csv_row`
   → identisch; und `has_bmp=false` → drei leere Felder.

Double-Asserts sind im native-Env bereits aktiviert
(`-D UNITY_INCLUDE_DOUBLE`).

## Nach der Umsetzung

`docs/software-flow.md` aktualisieren: neuer `lib`-Baustein `bmp280` → ✅, der
`READ_SENS`-Knoten im `loop()`-Diagramm teilweise grün, Landkarten-Tabelle in §6
ergänzen. (CLAUDE.md: Software-Flow am Ende der Arbeit mitpflegen.)

## Quellen

- Bosch **BMP280 Datasheet Rev. 1.26** (BST-BMP280-DS001, Okt. 2021), Abschnitte
  3.11.2 (Kalibriertypen) und 3.11.3 (32-bit-Kompensationsformel). Lokal
  extrahiert und Formel bit-genau nachgerechnet.
- Community-Testkonstanten zur Gegenbestätigung: mahfuz195/BMP280-Arduino-Library.
