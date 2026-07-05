### Task 4: CSV-Integration (3-Stellen-Erweiterung)

Nimmt `has_bmp` + `temp_c`, `pressure_hpa`, `alt_baro_m` in den `TelemetryRecord` auf und erweitert `csv_header`, `csv_row`, `parse_csv_row`. Round-Trip-Absicherung.

**Files:**
- Modify: `lib/telemetry/record.h` (Felder ergänzen)
- Modify: `lib/telemetry/record.cpp` (`csv_header`, `csv_row`, `parse_csv_row`)
- Modify: `test/test_record/test_record.cpp` (Tests + `RUN_TEST`, bestehenden Test anpassen)

**Interfaces:**
- Consumes: nichts aus Task 1–3 (Record speichert fertige °C/hPa/m-Werte; die Umrechnung macht später `src/flight`).
- Produces: erweitertes CSV-Format mit Spalten `...,sats,temp_c,pressure_hpa,alt_baro_m` (10 Spalten statt 7).

- [ ] **Step 1: Failing tests schreiben + bestehenden Spaltenzahl-Test anpassen**

Modify `test/test_record/test_record.cpp`:

**(a)** Den bestehenden Test `test_wrong_column_count_rejected` anpassen — die neue Spaltenzahl ist 11, eine 7-Spalten-Zeile muss jetzt abgelehnt werden:

```cpp
// Falsche Spaltenzahl (7 statt 10) -> abgelehnt.
void test_wrong_column_count_rejected() {
    TelemetryRecord out;
    // alte 7-Spalten-Zeile ohne BMP-Felder
    TEST_ASSERT_FALSE(parse_csv_row("123,,PREFLIGHT,,,,", out));
}
```

**(b)** Neue Tests vor `main()` einfügen:

```cpp
// Header endet mit den drei BMP280-Spalten in fester Reihenfolge.
void test_header_ends_with_bmp_columns() {
    std::string h = csv_header();
    std::string suffix = "temp_c,pressure_hpa,alt_baro_m";
    TEST_ASSERT_TRUE(h.size() >= suffix.size());
    TEST_ASSERT_EQUAL_STRING(suffix.c_str(),
                             h.substr(h.size() - suffix.size()).c_str());
}

// Ohne BMP-Sensor bleiben die drei Spalten leer (kein 0.0 vortäuschen).
void test_row_without_bmp_has_empty_fields() {
    TelemetryRecord r;
    r.t_ms = 1;
    r.has_bmp = false;
    std::string row = csv_row(r);
    // Zeile endet mit drei leeren Feldern: ",,"  (temp, pressure, alt)
    TEST_ASSERT_EQUAL_STRING(",,",
                             row.substr(row.size() - 2).c_str());
}

// Round-Trip MIT BMP-Werten.
void test_roundtrip_with_bmp() {
    TelemetryRecord in;
    in.t_ms = 3000;
    in.phase = Phase::Ascent;
    in.has_bmp = true;
    in.temp_c = 25.08f;
    in.pressure_hpa = 1006.56f;
    in.alt_baro_m = 55.85f;

    TelemetryRecord out;
    TEST_ASSERT_TRUE(parse_csv_row(csv_row(in), out));
    TEST_ASSERT_TRUE(out.has_bmp);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, in.temp_c, out.temp_c);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, in.pressure_hpa, out.pressure_hpa);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, in.alt_baro_m, out.alt_baro_m);
}

// Round-Trip OHNE BMP -> has_bmp false zurück.
void test_roundtrip_without_bmp() {
    TelemetryRecord in;
    in.t_ms = 4000;
    in.has_bmp = false;

    TelemetryRecord out;
    out.has_bmp = true;  // bewusst vorbelegen, muss überschrieben werden
    TEST_ASSERT_TRUE(parse_csv_row(csv_row(in), out));
    TEST_ASSERT_FALSE(out.has_bmp);
}
```

In `main()` ergänzen (nach den bestehenden `RUN_TEST`):

```cpp
    RUN_TEST(test_header_ends_with_bmp_columns);
    RUN_TEST(test_row_without_bmp_has_empty_fields);
    RUN_TEST(test_roundtrip_with_bmp);
    RUN_TEST(test_roundtrip_without_bmp);
```

- [ ] **Step 2: Test laufen lassen und Fehlschlag verifizieren**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && pio test -e native -f test_record`
Expected: FAIL — Kompilierfehler (`has_bmp`/`temp_c`/`pressure_hpa`/`alt_baro_m` sind keine Member von `TelemetryRecord`).

- [ ] **Step 3: Record-Felder ergänzen (Stelle 1)**

Modify `lib/telemetry/record.h` — die Kommentarzeile für weitere Sensoren ersetzen durch:

```cpp
    // --- BMP280 (Temperatur/Druck/Barometer-Höhe), ein gemeinsames Flag ---
    bool  has_bmp = false;
    float temp_c = 0.0f;        // °C
    float pressure_hpa = 0.0f;  // hPa
    float alt_baro_m = 0.0f;    // barometrische Höhe (m), aus Druck + QNH
```

- [ ] **Step 4: `csv_header` erweitern (Stelle 2)**

Modify `lib/telemetry/record.cpp` — `csv_header()`:

```cpp
std::string csv_header() {
    return "t_ms,utc,phase,lat,lon,alt_gps_m,sats,temp_c,pressure_hpa,alt_baro_m";
}
```

- [ ] **Step 5: `csv_row` erweitern (Stelle 3a)**

Modify `lib/telemetry/record.cpp` — in `csv_row()` **vor** `return s;` einfügen:

```cpp
    s += ',';
    // BMP280: nur bei bestücktem Sensor Werte, sonst drei leere Felder.
    if (r.has_bmp) {
        s += num(r.temp_c, 2);       s += ',';
        s += num(r.pressure_hpa, 2); s += ',';
        s += num(r.alt_baro_m, 2);
    } else {
        s += ",,";  // temp_c, pressure_hpa, alt_baro_m alle leer
    }
```

- [ ] **Step 6: `parse_csv_row` erweitern (Stelle 3b)**

Modify `lib/telemetry/record.cpp` — in `parse_csv_row()`:

Die Spaltenzahl-Prüfung von 7 auf 11 ändern:

```cpp
    if (f.size() != 10) return false;          // Spaltenzahl muss passen
```

Vor `return true;` einfügen:

```cpp
    // BMP280 (Felder [7]/[8]/[9]): gilt als vorhanden, wenn temp_c befüllt ist.
    if (!f[7].empty()) {
        out.has_bmp = true;
        out.temp_c = static_cast<float>(std::strtod(f[7].c_str(), nullptr));
        if (!f[8].empty()) out.pressure_hpa = static_cast<float>(std::strtod(f[8].c_str(), nullptr));
        if (!f[9].empty()) out.alt_baro_m   = static_cast<float>(std::strtod(f[9].c_str(), nullptr));
    }
```

- [ ] **Step 7: Test laufen lassen und Erfolg verifizieren**

Run: `pio test -e native -f test_record`
Expected: PASS — alle Record-Tests grün (13 alte, davon 1 angepasst + 4 neue).

- [ ] **Step 8: Gesamte native Suite laufen lassen**

Run: `pio test -e native`
Expected: PASS — alle Test-Ordner grün (bestehende 20 Tests + bmp280 + neue record-Tests).

- [ ] **Step 9: Commit (optional, nur falls Git-Repo)**

```bash
git rev-parse --is-inside-work-tree >/dev/null 2>&1 && \
  git add lib/telemetry/record.h lib/telemetry/record.cpp test/test_record/test_record.cpp && \
  git commit -m "feat(record): BMP280-Spalten (temp_c, pressure_hpa, alt_baro_m) ins CSV"
```

---

