### Task 1: BMP280-Temperatur-Kompensation + Modulgerüst

Baut den Modul-Header `bmp280.h` (Calib-Struct + drei Funktionssignaturen) und implementiert die Temperatur-Kompensation. Enthält den Vorzeichen-Fallen-Test, weil die Temperatur-Konstanten (`dig_T3=-1000`) signed sind.

**Files:**
- Create: `lib/telemetry/bmp280.h`
- Create: `lib/telemetry/bmp280.cpp`
- Test: `test/test_bmp280/test_bmp280.cpp`

**Interfaces:**
- Consumes: nichts (neues Modul).
- Produces:
  - `struct telemetry::Bmp280Calib { uint16_t dig_T1; int16_t dig_T2, dig_T3; uint16_t dig_P1; int16_t dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9; };`
  - `int32_t telemetry::bmp280_compensate_temperature(int32_t adc_T, const Bmp280Calib& c, int32_t& t_fine);` — Rückgabe in 0,01 °C; setzt `t_fine`.
  - `uint32_t telemetry::bmp280_compensate_pressure(int32_t adc_P, const Bmp280Calib& c, int32_t t_fine);` — Rückgabe in Pa (implementiert in Task 2).
  - `float telemetry::bmp280_altitude_m(float pressure_pa, float sea_level_pa);` — Rückgabe in m (implementiert in Task 3).

- [ ] **Step 1: Header mit vollständiger API anlegen**

Create `lib/telemetry/bmp280.h`:

```cpp
// bmp280.h — BMP280-Umrechnung: Rohwerte + Kalibrierung -> physikalische Größen.
//
// Hardware-frei (kein <Arduino.h>) -> nativ testbar. Kennt NUR den BMP280,
// nicht das CSV-Format und KEIN I²C. Der src/flight-Aufrufer liest per I²C die
// Kalibrierkonstanten (einmalig) und die Rohwerte (pro Zyklus) und ruft diese
// Funktionen. Formeln exakt nach BST-BMP280-DS001 Rev 1.26, 3.11.3 (32-bit).

#ifndef TELEMETRY_BMP280_H
#define TELEMETRY_BMP280_H

#include <cstdint>

namespace telemetry {

// Kalibrierkonstanten (Register 0x88..0x9F). Typen exakt nach Datenblatt
// (3.11.2) — Vorzeichen entscheidend, sonst grob falsche Werte!
struct Bmp280Calib {
    uint16_t dig_T1;
    int16_t  dig_T2, dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2, dig_P3, dig_P4, dig_P5,
             dig_P6, dig_P7, dig_P8, dig_P9;
};

// adc_T = 20-bit-Rohwert (positiv, in 32-bit-signed). Rückgabe in 0,01 °C
// (2508 = 25,08 °C). Setzt t_fine (Zwischengröße für die Druckformel).
int32_t bmp280_compensate_temperature(int32_t adc_T, const Bmp280Calib& c,
                                       int32_t& t_fine);

// adc_P = 20-bit-Rohwert. t_fine MUSS vorher von compensate_temperature stammen.
// Rückgabe in Pa direkt (100656 = 1006,56 hPa).
uint32_t bmp280_compensate_pressure(int32_t adc_P, const Bmp280Calib& c,
                                     int32_t t_fine);

// Barometrische Höhe (m) aus Druck (Pa) + Referenzdruck QNH (Pa).
float bmp280_altitude_m(float pressure_pa, float sea_level_pa);

} // namespace telemetry

#endif // TELEMETRY_BMP280_H
```

- [ ] **Step 2: Failing test für Temperatur + Vorzeichen-Falle schreiben**

Create `test/test_bmp280/test_bmp280.cpp`:

```cpp
// test_bmp280.cpp — native Tests für die BMP280-Umrechnung.
//   pio test -e native
//
// Referenzwerte verifiziert aus dem Datenblatt-C-Code (BST-BMP280-DS001
// Rev 1.26, 3.11.3), NICHT aus dem Gedächtnis. Kalibriersatz + Ergebnisse:
//   adc_T=519888 -> t_fine=128422, T=2508 (25,08 °C)
//   adc_P=415148 -> 100656 Pa (1006,56 hPa)

#include <unity.h>
#include "bmp280.h"

using namespace telemetry;

void setUp() {}
void tearDown() {}

// Klassischer, in mehreren Bibliotheken zitierter Kalibriersatz.
static Bmp280Calib ref_calib() {
    Bmp280Calib c;
    c.dig_T1 = 27504;  c.dig_T2 = 26435;  c.dig_T3 = -1000;
    c.dig_P1 = 36477;  c.dig_P2 = -10685; c.dig_P3 = 3024;
    c.dig_P4 = 2855;   c.dig_P5 = 140;    c.dig_P6 = -7;
    c.dig_P7 = 15500;  c.dig_P8 = -14600; c.dig_P9 = 6000;
    return c;
}

// Temperatur gegen den verifizierten Referenzwert (25,08 °C) + t_fine.
void test_temperature_reference() {
    Bmp280Calib c = ref_calib();
    int32_t t_fine = 0;
    int32_t T = bmp280_compensate_temperature(519888, c, t_fine);
    TEST_ASSERT_EQUAL_INT32(128422, t_fine);
    TEST_ASSERT_EQUAL_INT32(2508, T);   // 25,08 °C
}

// Vorzeichen-Falle: negative Kalibrierkonstanten müssen als signed wirken.
// Würde dig_T3=-1000 fälschlich als uint16 (65536-1000) gelesen, käme ein
// grob anderer t_fine heraus -> dieser Test schlägt dann fehl.
void test_temperature_signed_calib() {
    Bmp280Calib c = ref_calib();
    int32_t t_fine = 0;
    bmp280_compensate_temperature(519888, c, t_fine);
    TEST_ASSERT_EQUAL_INT32(128422, t_fine);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_temperature_reference);
    RUN_TEST(test_temperature_signed_calib);
    return UNITY_END();
}
```

- [ ] **Step 3: Test laufen lassen und Fehlschlag verifizieren**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && pio test -e native -f test_bmp280`
Expected: FAIL — Linker-Fehler „undefined reference to `bmp280_compensate_temperature`" (Funktion noch nicht implementiert).

- [ ] **Step 4: Temperatur-Kompensation implementieren**

Create `lib/telemetry/bmp280.cpp`:

```cpp
// bmp280.cpp — BMP280-Umrechnung (siehe bmp280.h). Hardware-frei.
// Integer-Kompensation exakt nach BST-BMP280-DS001 Rev 1.26, 3.11.3.

#include "bmp280.h"

namespace telemetry {

int32_t bmp280_compensate_temperature(int32_t adc_T, const Bmp280Calib& c,
                                       int32_t& t_fine) {
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)c.dig_T1 << 1))) *
                    ((int32_t)c.dig_T2)) >> 11;
    int32_t var2 = (((((adc_T >> 4) - ((int32_t)c.dig_T1)) *
                      ((adc_T >> 4) - ((int32_t)c.dig_T1))) >> 12) *
                    ((int32_t)c.dig_T3)) >> 14;
    t_fine = var1 + var2;
    return (t_fine * 5 + 128) >> 8;
}

} // namespace telemetry
```

- [ ] **Step 5: Test laufen lassen und Erfolg verifizieren**

Run: `pio test -e native -f test_bmp280`
Expected: PASS — 2 Tests grün (`test_temperature_reference`, `test_temperature_signed_calib`).

- [ ] **Step 6: Commit (optional, nur falls Git-Repo)**

```bash
git rev-parse --is-inside-work-tree >/dev/null 2>&1 && \
  git add lib/telemetry/bmp280.h lib/telemetry/bmp280.cpp test/test_bmp280/test_bmp280.cpp && \
  git commit -m "feat(bmp280): Temperatur-Kompensation (Bosch int32) + Modulgerüst"
```

---

