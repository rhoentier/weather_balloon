# Design: OLED-Sensor-Status (Boden-Check)

**Datum:** 2026-07-04
**Status:** Design freigegeben, Umsetzung offen
**Board:** Heltec WiFi LoRa 32 V2 (internes SSD1306 128×64)

---

## 1. Zweck

Das board-interne OLED zeigt **vor dem Start** auf einen Blick, **welche
Komponenten funktionieren** — ohne Laptop/seriellen Monitor. Klassischer
„Startklar?"-Check kurz vor dem Loslassen.

Sobald der Ballon steigt (Flugphasen-Übergang `PreFlight → Ascent`), schaltet
die Firmware das Display ab: Im Flug sieht die Anzeige niemand, und bei 2–3 h
Flugzeit mit knappem Energiebudget spart das ~10–20 mA.

## 2. Scope (YAGNI)

Angezeigt werden **nur Komponenten, die die Firmware heute wirklich abfragt**:

- **GPS** — zweistufig (siehe unten)
- **microSD** — Ergebnis von `sd_log_begin()`

**Bewusst NICHT enthalten:** BMP280, MPU-6050, DS18B20, UV. Diese sind
verdrahtet/gekauft, aber noch nicht in der Firmware angebunden. Gemäß CLAUDE.md
(„keine leeren Sensor-Platzhalter") bekommt ein Sensor erst dann eine
Statuszeile, wenn er tatsächlich gelesen wird.

### GPS zweistufig

| Zustand | Kriterium | Anzeige |
|---|---|---|
| `Silent`  | seit Boot noch **nie** eine GGA geparst | `GPS: --` |
| `Waiting` | GGA kommt, aber `has_fix == false`       | `GPS: warte...` |
| `Fix`     | `has_fix == true`                        | `GPS: Fix N Sat` |

Der Unterschied „Modul stumm" vs. „wartet nur noch auf Satelliten" ist beim
Boden-Check genau die gebrauchte Information. Alle Werte (`has_fix`, `sats`)
liegen im `TelemetryRecord` bereits vor.

### Beispiel-Layout (128×64)

```
Wetterballon
GPS:  Fix 7 Sat
SD:   ok
Phase: PreFlight
```

## 3. Architektur (Logik/Hardware-Trennung)

Kernregel des Projekts: Testbares nach `lib/`, Hardware nach `src/`.
Das **Formatieren** der Statuszeilen ist reine Logik (nativ testbar), das
**Zeichnen + Vext-Schalten** ist Hardware (am Board verifiziert).

### `lib/telemetry/display_status.h/.cpp` — hardware-frei, nativ getestet (TDD)

```cpp
namespace telemetry {

enum class GpsDisp { Silent, Waiting, Fix };

struct DisplayState {
    GpsDisp gps;
    uint8_t sats;
    bool    sd_ok;
    Phase   phase;
};

// Liefert die anzuzeigenden Textzeilen (Reihenfolge = Layout oben).
std::vector<std::string> status_lines(const DisplayState&);

}
```

Reine Textformatierung → läuft als nativer Unit-Test auf dem Mac in ms.
Kein `<Arduino.h>`, kein U8g2.

### `src/flight/oled.h/.cpp` — Hardware, am Board verifiziert

- `oled_begin()` — Vext an (**GPIO21 LOW**), kurz warten, U8g2 initialisieren.
- `oled_show(const std::vector<std::string>&)` — Zeilen zeichnen.
- `oled_off()` — Display in Power-Save + Vext aus (**GPIO21 HIGH**).

U8g2-Konstruktor (verifiziert, V2-Pins aus `pins.h`):
```cpp
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0,
    /*reset=*/16, /*clock=*/PIN_I2C_SCL /*15*/, /*data=*/PIN_I2C_SDA /*4*/);
```

### `src/flight/main.cpp` — verdrahtet beides

- `setup()`: `oled_begin()`.
- `loop()`: solange Phase `PreFlight` → `DisplayState` aus vorhandenem `g_rec`
  + SD-Status bauen → `status_lines()` → `oled_show()`.
- Beim einmaligen Übergang `PreFlight → Ascent`: `oled_off()`.

**GPS-Stufen-Ableitung:** Ein „schon mal GGA gesehen?"-Flag lebt in `main.cpp`
(hardware-nah). `has_fix` → `Fix`, sonst gesehen → `Waiting`, sonst `Silent`.
Die Formatierung selbst bleibt in `lib/`.

## 4. Fehlerbehandlung / Robustheit

- **OLED nicht gefunden:** Betrieb läuft weiter (Logging ist die Hauptaufgabe).
  U8g2 blockiert nicht; ein fehlendes Display darf den Flug nicht stoppen —
  analog zu `sd_log` („Betrieb läuft auch ohne").
- **SD-Status:** wird beim Boot einmal ermittelt und mitgeführt.
- **Vext/OLED-Kopplung — offener Board-Punkt:** Ob das OLED beim V2 *zwingend*
  über Vext hängt, ist in der offiziellen Heltec-FAQ/Datenblatt nicht eindeutig
  belegt (Community-Quellen nehmen es an). Praktisch unkritisch: Vext wird beim
  Boot ohnehin **an** geschaltet (nötig für BMP280/MPU an derselben Rail), beim
  Flugstart **aus**. Ob das OLED dann real dunkel wird, ist ein Punkt der
  Board-Verifikation, kein Design-Blocker.

## 5. Tests

**Nativ (`test/test_display_status/`, TDD):**
- `Silent` → `GPS: --`
- `Waiting` → `GPS: warte...`
- `Fix` mit N Sats → `GPS: Fix N Sat`
- `sd_ok` true/false → `SD: ok` / `SD: --`
- Phasenzeile nutzt `to_string(Phase)`
- Zeilenanzahl/-reihenfolge stabil (Layout-Kontrakt)

**Am Board (Erweiterung der TODO-Testreihenfolge):**
- OLED zeigt Boden-Check, Werte plausibel.
- Beim Übergang in `Ascent` geht das Display aus (Vext-Abschaltung prüfen).

## 6. Verifizierte Fakten & Quellen

- U8g2-Konstruktor V2 (clock=15, data=4, reset=16):
  <https://github.com/olikraus/u8g2/discussions/1586>
- Vext = GPIO21, LOW=an / HIGH=aus:
  <https://docs.heltec.org/en/node/esp32/wifi_lora_32/frequently_asked_questions.html>
- V2-Pinout (PDF):
  <https://resource.heltec.cn/download/WiFi_LoRa_32/WIFI_LoRa_32_V2.pdf>

## 7. Betroffene Dateien / Software-Flow

- **Neu:** `lib/telemetry/display_status.h/.cpp`, `src/flight/oled.h/.cpp`,
  `test/test_display_status/`
- **Geändert:** `src/flight/main.cpp` (OLED einbinden + bei Ascent abschalten)
- **Nachziehen:** `docs/software-flow.md` (OLED-Baustein + Statuswechsel)
- **U8g2** als lib_deps im `flight`-Environment (`platformio.ini`)
