### Task 2: BMP280-Druck-Kompensation

Implementiert die Druckformel, die `t_fine` aus Task 1 nutzt und Pa direkt zurückgibt.

**Files:**
- Modify: `lib/telemetry/bmp280.cpp` (Funktion `bmp280_compensate_pressure` ergänzen)
- Modify: `test/test_bmp280/test_bmp280.cpp` (Test + `RUN_TEST` ergänzen)

**Interfaces:**
- Consumes: `bmp280_compensate_temperature(...)` (für `t_fine`), `Bmp280Calib` — beide aus Task 1.
- Produces: `uint32_t bmp280_compensate_pressure(int32_t adc_P, const Bmp280Calib& c, int32_t t_fine);` — Rückgabe in Pa.

- [ ] **Step 1: Failing test für Druck schreiben**

Modify `test/test_bmp280/test_bmp280.cpp` — Test-Funktion **vor** `main()` einfügen:

```cpp
// Druck gegen den verifizierten Referenzwert (100656 Pa). Nutzt t_fine aus der
// Temperatur-Kompensation — Reihenfolge wie im echten loop().
void test_pressure_reference() {
    Bmp280Calib c = ref_calib();
    int32_t t_fine = 0;
    bmp280_compensate_temperature(519888, c, t_fine);   // füllt t_fine
    uint32_t p = bmp280_compensate_pressure(415148, c, t_fine);
    TEST_ASSERT_EQUAL_UINT32(100656, p);   // 1006,56 hPa
}
```

In `main()` die Zeile ergänzen (nach den bestehenden `RUN_TEST`):

```cpp
    RUN_TEST(test_pressure_reference);
```

- [ ] **Step 2: Test laufen lassen und Fehlschlag verifizieren**

Run: `pio test -e native -f test_bmp280`
Expected: FAIL — Linker-Fehler „undefined reference to `bmp280_compensate_pressure`".

- [ ] **Step 3: Druck-Kompensation implementieren**

Modify `lib/telemetry/bmp280.cpp` — Funktion **vor** dem schließenden `} // namespace telemetry` einfügen:

```cpp
uint32_t bmp280_compensate_pressure(int32_t adc_P, const Bmp280Calib& c,
                                     int32_t t_fine) {
    int32_t var1 = (((int32_t)t_fine) >> 1) - (int32_t)64000;
    int32_t var2 = (((var1 >> 2) * (var1 >> 2)) >> 11) * ((int32_t)c.dig_P6);
    var2 = var2 + ((var1 * ((int32_t)c.dig_P5)) << 1);
    var2 = (var2 >> 2) + (((int32_t)c.dig_P4) << 16);
    var1 = (((c.dig_P3 * (((var1 >> 2) * (var1 >> 2)) >> 13)) >> 3) +
            ((((int32_t)c.dig_P2) * var1) >> 1)) >> 18;
    var1 = ((((32768 + var1)) * ((int32_t)c.dig_P1)) >> 15);
    if (var1 == 0) {
        return 0;  // Division durch Null vermeiden
    }
    uint32_t p = (((uint32_t)(((int32_t)1048576) - adc_P) - (var2 >> 12))) * 3125;
    if (p < 0x80000000) {
        p = (p << 1) / ((uint32_t)var1);
    } else {
        p = (p / (uint32_t)var1) * 2;
    }
    var1 = (((int32_t)c.dig_P9) * ((int32_t)(((p >> 3) * (p >> 3)) >> 13))) >> 12;
    var2 = (((int32_t)(p >> 2)) * ((int32_t)c.dig_P8)) >> 13;
    p = (uint32_t)((int32_t)p + ((var1 + var2 + c.dig_P7) >> 4));
    return p;
}
```

- [ ] **Step 4: Test laufen lassen und Erfolg verifizieren**

Run: `pio test -e native -f test_bmp280`
Expected: PASS — 3 Tests grün.

- [ ] **Step 5: Commit (optional, nur falls Git-Repo)**

```bash
git rev-parse --is-inside-work-tree >/dev/null 2>&1 && \
  git add lib/telemetry/bmp280.cpp test/test_bmp280/test_bmp280.cpp && \
  git commit -m "feat(bmp280): Druck-Kompensation (Bosch int32, Pa direkt)"
```

---

