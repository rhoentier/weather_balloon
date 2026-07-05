### Task 3: Barometrische Höhe

Implementiert die internationale Höhenformel mit QNH als Parameter.

**Files:**
- Modify: `lib/telemetry/bmp280.cpp` (Funktion `bmp280_altitude_m` ergänzen, `<cmath>` einbinden)
- Modify: `test/test_bmp280/test_bmp280.cpp` (Tests + `RUN_TEST` ergänzen)

**Interfaces:**
- Consumes: nichts Neues.
- Produces: `float bmp280_altitude_m(float pressure_pa, float sea_level_pa);` — Rückgabe in m.

- [ ] **Step 1: Failing tests für Höhe schreiben**

Modify `test/test_bmp280/test_bmp280.cpp` — vor `main()` einfügen:

```cpp
// Höhe: gleicher Druck wie Referenzdruck -> 0 m.
void test_altitude_zero_at_sea_level() {
    float h = bmp280_altitude_m(101325.0f, 101325.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, h);
}

// Höhe: verifizierter Standardpunkt (p=89876 Pa, QNH=101325 Pa) -> ~1000 m.
void test_altitude_reference_point() {
    float h = bmp280_altitude_m(89876.0f, 101325.0f);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 1000.0f, h);   // 1000,02 m ± 1 m
}
```

In `main()` ergänzen:

```cpp
    RUN_TEST(test_altitude_zero_at_sea_level);
    RUN_TEST(test_altitude_reference_point);
```

- [ ] **Step 2: Test laufen lassen und Fehlschlag verifizieren**

Run: `pio test -e native -f test_bmp280`
Expected: FAIL — Linker-Fehler „undefined reference to `bmp280_altitude_m`".

- [ ] **Step 3: Höhenformel implementieren**

Modify `lib/telemetry/bmp280.cpp`:

Oben, direkt nach `#include "bmp280.h"`, ergänzen:

```cpp
#include <cmath>
```

Vor dem schließenden `} // namespace telemetry` einfügen:

```cpp
float bmp280_altitude_m(float pressure_pa, float sea_level_pa) {
    // Internationale barometrische Höhenformel (Standardatmosphäre).
    return 44330.0f * (1.0f - std::pow(pressure_pa / sea_level_pa,
                                       1.0f / 5.255f));
}
```

- [ ] **Step 4: Test laufen lassen und Erfolg verifizieren**

Run: `pio test -e native -f test_bmp280`
Expected: PASS — 5 Tests grün.

- [ ] **Step 5: Commit (optional, nur falls Git-Repo)**

```bash
git rev-parse --is-inside-work-tree >/dev/null 2>&1 && \
  git add lib/telemetry/bmp280.cpp test/test_bmp280/test_bmp280.cpp && \
  git commit -m "feat(bmp280): barometrische Höhe (QNH als Parameter)"
```

---

