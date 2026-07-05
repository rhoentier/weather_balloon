# Design: OLED-Sensorstatuszeile (`lib/telemetry/display_status`)

Datum: 2026-07-04
Status: abgesegnet, bereit für Implementierungsplan

## Zweck & Grenze

Der bestehende OLED-„Startklar?“-Check (`lib/telemetry/display_status`, hardware-
frei, nativ getestet) zeigt heute vier Zeilen: Titel, GPS-Stufe, SD-Status,
Flugphase. Diese Iteration ergänzt eine **fünfte Zeile**, die auf einen Blick
zeigt, welche der vier Sensoren (BMP280, MPU-6050, DS18B20, GUVA-S12SD/UV)
gerade **ok** sind — analog zum bestehenden `SD: ok` / `SD: --`-Muster.

**Ausdrücklich außerhalb dieser Iteration:**
- Keine I²C-/1-Wire-/ADC-Anbindung in `src/flight`. Die Sensoren MPU-6050,
  DS18B20 und UV werden dort aktuell noch gar nicht gelesen (siehe `TODO.md`,
  Schritt 2 „Sensor-Lesung + Auto-Detect“ ist offen); ihre Statusfelder bleiben
  vorerst fest `false`, statt ein „ok“ vorzutäuschen, das nicht existiert.
- Kein Update von `src/flight/main.cpp`/`oled.cpp`. Diese Iteration bleibt
  komplett im bereits hardware-frei getesteten Baustein `lib/telemetry/
  display_status` — passend zur bestehenden Trennung `lib/` (nativ testbar) vs.
  `src/flight` (Board-Verifikation). Das Verdrahten von `main.cpp` (BMP-Flag aus
  `g_rec.has_bmp` ableiten, MPU/DS18B20/UV vorerst `false`) ist ein eigener,
  späterer Schritt, sobald diese Sensoren tatsächlich angebunden werden.

## Interface (`lib/telemetry/display_status.h`)

```cpp
struct DisplayState {
    GpsDisp gps;
    uint8_t sats;
    bool    sd_ok;
    Phase   phase;
    bool    bmp_ok;       // NEU
    bool    mpu_ok;       // NEU
    bool    ds18b20_ok;   // NEU
    bool    uv_ok;        // NEU
};

// Anzuzeigende Zeilen. Reihenfolge = Layout-Kontrakt:
//   [0] Titel, [1] GPS, [2] SD, [3] Phase, [4] Sensoren.
std::vector<std::string> status_lines(const DisplayState& s);
```

### Bewusste Design-Entscheidungen

1. **Vier einzelne `bool`-Felder**, analog zu `sd_ok` — kein neuer Struct/Enum
   für „Sensor-Status“. Es bleibt bei genau vier fest benannten Sensoren; ein
   generisches Array/Struct wäre Abstraktion auf Vorrat (YAGNI-Grenze).
2. **Neue Zeile wird angehängt (Index 4), bestehende Indizes 0–3 bleiben
   unverändert.** Schützt bestehende Aufrufer und Tests vor Indexverschiebung.
3. **Kurzcodes statt ausgeschriebener Namen.** Bei 128 px Displaybreite und dem
   verwendeten Font (`u8g2_font_6x12_tf`, 6 px/Zeichen) passen ca. 21 Zeichen
   pro Zeile. Die ausgeschriebene Variante `BMP:ok MPU:-- DS:-- UV:--` wäre mit
   25 Zeichen zu breit und liefe über den Rand. Ein-Buchstaben-Kürzel
   (`B`=BMP280, `M`=MPU-6050, `D`=DS18B20, `U`=UV) halten die Zeile bei 19
   Zeichen und bleiben bei nur vier Sensoren eindeutig.

## Format der Sensorzeile

Reihenfolge fest: BMP280, MPU-6050, DS18B20, UV — feste, geräteweite Reihenfolge,
unabhängig von tatsächlicher Lese-Reihenfolge am Board.

```
B:ok M:-- D:-- U:--
```

Jeder Sensor unabhängig `ok` (Flag `true`) oder `--` (Flag `false`), mit genau
einem Leerzeichen zwischen den vier Kürzel-Wert-Paaren. Kein Sonderfall für
"alle ok" oder "alle fehlend" — jede Kombination folgt demselben Muster.

## Testreihenfolge (TDD, jeder Test erst RED sehen)

In `test/test_display_status/test_display_status.cpp`:

1. Bestehenden `test_layout_has_four_lines_in_order` auf **fünf** Zeilen
   erweitern: Assert auf `l.size() == 5` und dass `l[4]` mit `"B:"` beginnt.
   Bestehende Assertions für `l[0]..l[3]` bleiben unverändert.
2. **Alle vier Sensoren ok** → `"B:ok M:ok D:ok U:ok"`.
3. **Alle vier fehlend** → `"B:-- M:-- D:-- U:--"`.
4. **Gemischt** (z. B. nur `bmp_ok=true`, Rest `false`) → korrekte
   Einzelwerte je Position, um zu verifizieren, dass die vier Flags
   unabhängig voneinander ausgewertet werden (nicht nur ein „irgendeins ok“).

Alle Tests bleiben hardware-frei (`pio test -e native`), da `display_status`
kein `<Arduino.h>` kennt.

## Nach der Umsetzung

`docs/software-flow.md` aktualisieren: OLED-Boden-Check-Beschreibung (§3, §6)
um die neue Sensorzeile ergänzen; klarstellen, dass die vier Statusflags
aktuell nur für BMP280 potenziell befüllt werden können (sobald `main.cpp`
das in einem späteren Schritt verdrahtet) und für MPU-6050/DS18B20/UV fest
`false` bleiben, bis diese Sensoren angebunden sind.

## Quellen

Displaybreite/Zeichenbudget aus `src/flight/oled.h`/`oled.cpp` (128×64 SSD1306,
`u8g2_font_6x12_tf`, Zeilenabstand 13 px) und `docs/superpowers/specs/
2026-06-27-pinbelegung-heltec-v2.md`. Offener Anbindungsstand der Sensoren aus
`TODO.md`, Schritt 2.
