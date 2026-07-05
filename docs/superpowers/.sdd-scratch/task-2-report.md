# Task 2 Report: BMP280-Druck-Kompensation

Status: **DONE**

---

## Implementierung

Folgende Dateien wurden nach dem TDD-Prozess modifiziert:

1. **`test/test_bmp280/test_bmp280.cpp`**
   - Test `test_pressure_reference()` hinzugefügt (vor `main()`)
   - `RUN_TEST(test_pressure_reference)` in `main()` ergänzt

2. **`lib/telemetry/bmp280.cpp`**
   - Funktion `bmp280_compensate_pressure()` implementiert (vor `} // namespace telemetry`)
   - Formel exakt nach BST-BMP280-DS001 Rev 1.26, 3.11.3 (32-bit)

---

## Testverlauf

### Phase 1: RED (fehlgeschlagener Test)

Kommando:
```
pio test -e native -f test_bmp280
```

Ausgabe (Fehlschlag):
```
Undefined symbols for architecture arm64:
  "telemetry::bmp280_compensate_pressure(int, telemetry::Bmp280Calib const&, int)", referenced from:
      test_pressure_reference() in test_bmp280.o
ld: symbol(s) not found for architecture arm64
clang++: error: linker command failed with exit code 1 (use -v to be invocation)
*** [.pio/build/native/program] Error 1

[ERRORED] Took 0.46 seconds
```

**Analyse:** Undefined reference wie erwartet — die Funktion existiert nur in der Header-Datei (Task 1), die Implementierung fehlt noch.

---

### Phase 2: GREEN (erfolgreicher Test)

Kommando:
```
pio test -e native -f test_bmp280
```

Ausgabe (Erfolg):
```
test/test_bmp280/test_bmp280.cpp:58: test_temperature_reference	[PASSED]
test/test_bmp280/test_bmp280.cpp:59: test_temperature_signed_calib	[PASSED]
test/test_bmp280/test_bmp280.cpp:60: test_pressure_reference	[PASSED]

[PASSED] Took 0.86 seconds

3 test cases: 3 succeeded in 00:00:00.857
```

**Analyse:** Alle 3 Tests grün:
- `test_temperature_reference` (Task 1, baseline)
- `test_temperature_signed_calib` (Task 1, baseline)
- `test_pressure_reference` (Task 2, neu)

---

## Selbst-Review

### Referenzwert-Verifikation

Test erwartet: **100656 Pa** (1006,56 hPa)

Input-Daten (aus Datenblatt):
- `adc_T = 519888` → `t_fine = 128422` ✓
- `adc_P = 415148` (20-bit-Rohwert) → `100656 Pa` ✓

**Ergebnis:** Referenzwert korrekt getroffen. Die Formel produziert exakt den erwarteten Wert.

### Formel-Verifikation

Die implementierte Funktion folgt genau dem Datenblatt-C-Code:

1. **var1/var2 Berechnung:** Fußnote aus t_fine und Druckkalibrierkonstanten
2. **Division-by-Zero Check:** `if (var1 == 0) return 0;` verhindert Crash
3. **Overflow-Handling:** Zwei Pfade für `p < 0x80000000` vs. `p >= 0x80000000` (32-bit-sichere Division)
4. **Finale Korrektur:** var1/var2 mit dig_P8/dig_P9 und dig_P7-Offset

Bit-für-Bit-Vergleich mit Brief ✓ (keine Abweichungen)

### Abhängigkeiten

- ✓ `bmp280_compensate_temperature()` korrekt aufgerufen (setzt `t_fine`)
- ✓ `Bmp280Calib` mit allen 11 Druckkalibrierkonstanten vorhanden
- ✓ Rückgabewert `uint32_t` (Pa direkt, kein Shift)
- ✓ Signatur im Header (`bmp280.h`) matchet Implementierung

---

## Bedenken

**Keine.** Task ist vollständig und TDD-konform abgeschlossen.

---

## Geänderte Dateien (absolut)

- `/Users/steffenjendrny/weather_balloon/test/test_bmp280/test_bmp280.cpp`
- `/Users/steffenjendrny/weather_balloon/lib/telemetry/bmp280.cpp`

---

## Test-Zusammenfassung

**3 Tests grün (100%)** — Temperatur (Baseline Task 1) + Druck (Task 2).
