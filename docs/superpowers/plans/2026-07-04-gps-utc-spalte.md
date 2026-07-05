# GPS-UTC-Spalte Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Die GPS-Uhrzeit (UTC) aus der GGA-Zeile als neue CSV-Spalte `utc` (Format `hh:mm:ss`) ins Telemetrie-Format aufnehmen — als absoluter, neustartfester Zeitstempel neben dem monotonen `t_ms`.

**Architecture:** Zwei bestehende hardware-freie `lib`-Module werden erweitert. `lib/telemetry/gga` parst die UTC als ganzzahlige Rohwerte (`utc_h/min/s` + `has_utc`), unabhängig vom Positions-Fix. `lib/telemetry/record` bekommt die Spalte nach der bekannten 3-Stellen-Regel und formatiert sie als `hh:mm:ss`. Danach vier Zeilen Verdrahtung in `src/flight/main.cpp`.

**Tech Stack:** C++17, PlatformIO, Unity (native Tests auf dem Mac).

## Global Constraints

- **Hardware-frei bleiben:** kein `<Arduino.h>` in `lib/telemetry/` — muss unter `pio test -e native` laufen.
- **Sprache:** Kommentare/Doku auf Deutsch.
- **TDD:** jeder Logik-Schritt erst als fehlschlagender Test (RED), dann minimaler Code (GREEN), dann aufräumen.
- **YAGNI:** kein Datum, keine Zeitzone, keine Sekundenbruchteile, kein RTC.
- **CSV-Reihenfolge identisch** in `csv_header()` / `csv_row()` / `parse_csv_row()`. Neue Spalte `utc` steht **direkt nach `t_ms`**: `t_ms,utc,phase,lat,lon,alt_gps_m,sats`.
- **Fehlende Werte = leeres Feld** (nicht `0`).
- **Kein Git-Repo:** Dies ist aktuell kein Git-Repository. Die „Commit"-Schritte sind daher als *Checkpoint* zu verstehen (Tests grün, Stand sichern) — überspringe den `git`-Befehl, bis das Projekt initialisiert ist. Wenn `git` verfügbar wird, die Commit-Befehle wie angegeben nutzen.
- **Testlauf:** PlatformIO-CLI liegt unter `~/.platformio/penv/bin/`. Vor Testläufen im Terminal: `export PATH="$PATH:$HOME/.platformio/penv/bin"`.

---

### Task 1: UTC-Parsing im GGA-Parser

**Files:**
- Modify: `lib/telemetry/gga.h` (Felder in `GpsFix`)
- Modify: `lib/telemetry/gga.cpp` (Feld [1] parsen)
- Test: `test/test_gga/test_gga.cpp` (neue Testfälle + Registrierung in `main()`)

**Interfaces:**
- Consumes: bestehende `parse_gga(const std::string&, GpsFix&)`, `struct GpsFix`.
- Produces: `GpsFix` erhält vier neue Felder:
  - `bool has_utc = false;`
  - `uint8_t utc_h = 0;` (0..23)
  - `uint8_t utc_min = 0;` (0..59)
  - `uint8_t utc_s = 0;` (0..59)
  `parse_gga` setzt `has_utc=true` und die drei Werte, wenn GGA-Feld [1] mindestens 6 Ziffern hat; sonst `has_utc=false`. Entkoppelt von `has_fix`.

- [ ] **Step 1: Felder in `GpsFix` ergänzen (gga.h)**

In `lib/telemetry/gga.h` den Struct `GpsFix` um die UTC-Felder erweitern (nach `sats`):

```cpp
struct GpsFix {
    bool    has_fix = false;   // true = gültiger Satellitenfix (Fix-Qualität > 0)
    double  lat = 0.0;         // Dezimalgrad; N/O positiv, S/W negativ
    double  lon = 0.0;
    float   alt_gps_m = 0.0f;  // Höhe über Meer (GGA-Feld, "M")
    uint8_t sats = 0;          // Anzahl genutzter Satelliten

    // --- UTC-Uhrzeit aus GGA-Feld [1] (hhmmss), unabhängig vom Fix ---
    bool    has_utc = false;   // true = gültige Uhrzeit geparst
    uint8_t utc_h = 0;         // 0..23
    uint8_t utc_min = 0;       // 0..59
    uint8_t utc_s = 0;         // 0..59 (Nachkommastellen verworfen)
};
```

- [ ] **Step 2: Failing Tests schreiben (test_gga.cpp)**

In `test/test_gga/test_gga.cpp` diese sechs Testfunktionen VOR `main()` einfügen:

```cpp
// UTC aus GGA-Feld [1] "123519" -> 12:35:19.
void test_utc_parsed() {
    GpsFix fix;
    TEST_ASSERT_TRUE(parse_gga(GGA_VALID, fix));
    TEST_ASSERT_TRUE(fix.has_utc);
    TEST_ASSERT_EQUAL_UINT8(12, fix.utc_h);
    TEST_ASSERT_EQUAL_UINT8(35, fix.utc_min);
    TEST_ASSERT_EQUAL_UINT8(19, fix.utc_s);
}

// Führende Nullen: "080509" -> 8:5:9 (nicht an der 0 stolpern).
// Checksumme für "GPGGA,080509,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,," = 0x4F
void test_utc_leading_zeros() {
    GpsFix fix;
    TEST_ASSERT_TRUE(parse_gga(
        "$GPGGA,080509,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*4F",
        fix));
    TEST_ASSERT_TRUE(fix.has_utc);
    TEST_ASSERT_EQUAL_UINT8(8, fix.utc_h);
    TEST_ASSERT_EQUAL_UINT8(5, fix.utc_min);
    TEST_ASSERT_EQUAL_UINT8(9, fix.utc_s);
}

// UTC gültig auch OHNE Positions-Fix (Fix-Qualität 0).
// Zeile identisch zu test_quality_zero_no_fix (*46).
void test_utc_without_fix() {
    GpsFix fix;
    TEST_ASSERT_TRUE(parse_gga(
        "$GPGGA,123519,4807.038,N,01131.000,E,0,08,0.9,545.4,M,46.9,M,,*46",
        fix));
    TEST_ASSERT_FALSE(fix.has_fix);
    TEST_ASSERT_TRUE(fix.has_utc);
    TEST_ASSERT_EQUAL_UINT8(12, fix.utc_h);
}

// Leeres UTC-Feld [1] -> has_utc false.
// Checksumme für "GPGGA,,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,," = 0x66
void test_utc_empty_field() {
    GpsFix fix;
    fix.has_utc = true;  // Vorbedingung, damit der Assert wirklich prüft
    TEST_ASSERT_TRUE(parse_gga(
        "$GPGGA,,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*66",
        fix));
    TEST_ASSERT_FALSE(fix.has_utc);
}

// Zu kurzes UTC-Feld (< 6 Ziffern) -> has_utc false.
// Checksumme für "GPGGA,1235,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,," = 0x03
void test_utc_too_short() {
    GpsFix fix;
    fix.has_utc = true;
    TEST_ASSERT_TRUE(parse_gga(
        "$GPGGA,1235,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*03",
        fix));
    TEST_ASSERT_FALSE(fix.has_utc);
}
```

Und in `main()` registrieren (vor `return UNITY_END();`):

```cpp
    RUN_TEST(test_utc_parsed);
    RUN_TEST(test_utc_leading_zeros);
    RUN_TEST(test_utc_without_fix);
    RUN_TEST(test_utc_empty_field);
    RUN_TEST(test_utc_too_short);
```

> **Hinweis zu den Checksummen:** Die `*HH`-Werte oben sind vorberechnet. Falls ein `parse_gga` einen dieser Sätze wider Erwarten als `false` ablehnt (Checksummenfehler statt der erwarteten UTC-Prüfung), die Checksumme mit dem bestehenden Muster nachrechnen: XOR aller Zeichen zwischen `$` und `*`. Die anderen Felder sind bewusst identisch zu `GGA_VALID`, nur Feld [1] variiert.

- [ ] **Step 3: Tests laufen lassen — müssen fehlschlagen (RED)**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && pio test -e native -f test_gga`
Expected: FAIL — `test_utc_parsed` etc. schlagen fehl, weil `has_utc` nie gesetzt wird (bleibt `false`) bzw. `utc_h` = 0. Das ist der richtige Grund.

- [ ] **Step 4: Parsing implementieren (gga.cpp)**

In `lib/telemetry/gga.cpp` eine Hilfsfunktion oberhalb von `parse_gga` einfügen:

```cpp
// GGA-Feld [1] "hhmmss(.ss)" -> Stunden/Minuten/Sekunden.
// Gültig nur, wenn die ersten 6 Zeichen Ziffern sind. Nachkomma wird verworfen.
// Rückgabe false -> Feld leer/zu kurz/keine Ziffern (Aufrufer setzt has_utc=false).
static bool parse_utc(const std::string& field,
                      uint8_t& h, uint8_t& m, uint8_t& s) {
    if (field.size() < 6) return false;
    for (int i = 0; i < 6; ++i)
        if (field[i] < '0' || field[i] > '9') return false;
    h = static_cast<uint8_t>((field[0] - '0') * 10 + (field[1] - '0'));
    m = static_cast<uint8_t>((field[2] - '0') * 10 + (field[3] - '0'));
    s = static_cast<uint8_t>((field[4] - '0') * 10 + (field[5] - '0'));
    return true;
}
```

In `parse_gga`, nach dem Lesen von `sats`/`alt`/Koordinaten (vor `return true;`), die UTC aus Feld `f[1]` ziehen:

```cpp
    // UTC aus Feld [1] (hhmmss), unabhängig vom Positions-Fix.
    out.has_utc = parse_utc(f[1], out.utc_h, out.utc_min, out.utc_s);
```

- [ ] **Step 5: Tests laufen lassen — müssen bestehen (GREEN)**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && pio test -e native -f test_gga`
Expected: PASS — alle GGA-Tests grün (die 11 alten + 5 neue = 16).

- [ ] **Step 6: Checkpoint / Commit**

Tests grün → Stand sichern. (Kein Git-Repo: `git`-Befehl überspringen, siehe Global Constraints.)

```bash
git add lib/telemetry/gga.h lib/telemetry/gga.cpp test/test_gga/test_gga.cpp
git commit -m "feat(gga): UTC-Uhrzeit aus GGA-Feld parsen (has_utc, utc_h/min/s)"
```

---

### Task 2: `utc`-Spalte im CSV-Record

**Files:**
- Modify: `lib/telemetry/record.h` (Felder in `TelemetryRecord`)
- Modify: `lib/telemetry/record.cpp` (Header, Row, Parse)
- Test: `test/test_record/test_record.cpp` (neue Tests + zwei bestehende anpassen)

**Interfaces:**
- Consumes: bestehende `csv_header()`, `csv_row()`, `parse_csv_row()`, `struct TelemetryRecord`.
- Produces: `TelemetryRecord` erhält `bool has_utc; uint8_t utc_h, utc_min, utc_s;`. CSV-Header wird `t_ms,utc,phase,lat,lon,alt_gps_m,sats` (7 Spalten). `csv_row` schreibt bei `has_utc` das Feld als `hh:mm:ss` (führende Nullen), sonst leer. `parse_csv_row` liest es zurück und erwartet nun **7** Spalten.

- [ ] **Step 1: Felder in `TelemetryRecord` ergänzen (record.h)**

In `lib/telemetry/record.h` nach `phase` (vor dem GPS-Block) einfügen:

```cpp
    uint32_t t_ms = 0;                  // monotone Bordzeit (immer vorhanden)
    Phase    phase = Phase::PreFlight;  // aktuelle Flugphase (immer vorhanden)

    // --- GPS-UTC-Uhrzeit (aus GGA), unabhängig vom Positions-Fix ---
    bool    has_utc = false;
    uint8_t utc_h = 0;
    uint8_t utc_min = 0;
    uint8_t utc_s = 0;
```

- [ ] **Step 2: Bestehende Tests an neues Format anpassen (test_record.cpp)**

Zwei bestehende Tests prüfen den alten Header/das alte Zeilenformat und würden sonst fälschlich fehlschlagen. Anpassen:

In `test_header_starts_with_time_and_phase`:
```cpp
void test_header_starts_with_time_and_phase() {
    std::string h = csv_header();
    TEST_ASSERT_EQUAL_INT(0, h.rfind("t_ms,utc,phase,", 0));  // beginnt mit ...
}
```

In `test_row_without_fix_has_empty_gps_fields` (Record ohne UTC → leeres utc-Feld, also `1234,,PREFLIGHT,`):
```cpp
    std::string row = csv_row(r);
    // beginnt mit Zeit + (leeres utc) + Phase
    TEST_ASSERT_EQUAL_INT(0, row.rfind("1234,,PREFLIGHT,", 0));
    // und enthält die vier leeren GPS-Felder am Ende: ",,,,"
    TEST_ASSERT_TRUE(row.find(",,,,") != std::string::npos);
```

- [ ] **Step 3: Neue Failing Tests schreiben (test_record.cpp)**

Diese Testfunktionen vor `main()` einfügen:

```cpp
// Header führt utc an zweiter Stelle.
void test_header_has_utc_second() {
    std::string h = csv_header();
    TEST_ASSERT_EQUAL_INT(0, h.rfind("t_ms,utc,", 0));
}

// Row mit UTC -> hh:mm:ss mit führenden Nullen.
void test_row_with_utc_formatted() {
    TelemetryRecord r;
    r.t_ms = 100;
    r.has_utc = true;
    r.utc_h = 8; r.utc_min = 5; r.utc_s = 9;
    std::string row = csv_row(r);
    TEST_ASSERT_TRUE(row.find("08:05:09") != std::string::npos);
}

// Row ohne UTC -> leeres Feld an Position 2 (direkt nach t_ms).
void test_row_without_utc_empty() {
    TelemetryRecord r;
    r.t_ms = 100;
    r.has_utc = false;
    std::string row = csv_row(r);
    TEST_ASSERT_EQUAL_INT(0, row.rfind("100,,", 0));  // t_ms, dann leeres utc
}

// Round-Trip MIT UTC.
void test_roundtrip_with_utc() {
    TelemetryRecord in;
    in.t_ms = 55000;
    in.phase = Phase::Ascent;
    in.has_utc = true;
    in.utc_h = 23; in.utc_min = 59; in.utc_s = 1;

    TelemetryRecord out;
    TEST_ASSERT_TRUE(parse_csv_row(csv_row(in), out));
    TEST_ASSERT_TRUE(out.has_utc);
    TEST_ASSERT_EQUAL_UINT8(23, out.utc_h);
    TEST_ASSERT_EQUAL_UINT8(59, out.utc_min);
    TEST_ASSERT_EQUAL_UINT8(1,  out.utc_s);
}

// Round-Trip OHNE UTC -> has_utc false zurück.
void test_roundtrip_without_utc() {
    TelemetryRecord in;
    in.t_ms = 6000;
    in.has_utc = false;

    TelemetryRecord out;
    out.has_utc = true;  // bewusst vorbelegen, muss überschrieben werden
    TEST_ASSERT_TRUE(parse_csv_row(csv_row(in), out));
    TEST_ASSERT_FALSE(out.has_utc);
}

// Falsche Spaltenzahl (6 statt 7) -> abgelehnt.
void test_wrong_column_count_rejected() {
    TelemetryRecord out;
    // alte 6-Spalten-Zeile ohne utc
    TEST_ASSERT_FALSE(parse_csv_row("123,PREFLIGHT,,,,", out));
}
```

Und in `main()` registrieren:

```cpp
    RUN_TEST(test_header_has_utc_second);
    RUN_TEST(test_row_with_utc_formatted);
    RUN_TEST(test_row_without_utc_empty);
    RUN_TEST(test_roundtrip_with_utc);
    RUN_TEST(test_roundtrip_without_utc);
    RUN_TEST(test_wrong_column_count_rejected);
```

- [ ] **Step 4: Tests laufen lassen — müssen fehlschlagen (RED)**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && pio test -e native -f test_record`
Expected: FAIL — neue utc-Tests schlagen fehl (Header hat kein `utc`, Zeile kein utc-Feld), und die angepassten Alt-Tests scheitern noch am unveränderten `record.cpp`.

- [ ] **Step 5: Header, Row und Parse implementieren (record.cpp)**

In `lib/telemetry/record.cpp`:

`csv_header()` → utc-Spalte nach t_ms:
```cpp
std::string csv_header() {
    return "t_ms,utc,phase,lat,lon,alt_gps_m,sats";
}
```

`csv_row()` → utc-Feld direkt nach t_ms schreiben (führende Nullen mit `snprintf`):
```cpp
std::string csv_row(const TelemetryRecord& r) {
    std::string s;
    s += std::to_string(r.t_ms);
    s += ',';
    // UTC: hh:mm:ss mit führenden Nullen, sonst leeres Feld.
    if (r.has_utc) {
        char buf[9];
        std::snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
                      r.utc_h, r.utc_min, r.utc_s);
        s += buf;
    }
    s += ',';
    s += to_string(r.phase);
    s += ',';
    // GPS: nur bei Fix Werte, sonst leere Felder.
    if (r.has_fix) {
        s += num(r.lat, 6);       s += ',';
        s += num(r.lon, 6);       s += ',';
        s += num(r.alt_gps_m, 1); s += ',';
        s += std::to_string(r.sats);
    } else {
        s += ",,,";  // lat, lon, alt_gps_m, sats alle leer
    }
    return s;
}
```

`parse_csv_row()` → 7 Spalten, utc aus Feld [1], restliche Felder um eins verschoben:
```cpp
bool parse_csv_row(const std::string& line, TelemetryRecord& out) {
    if (line.empty()) return false;
    auto f = split(line);
    if (f.size() != 7) return false;          // Spaltenzahl muss passen
    if (f[0].empty()) return false;           // t_ms ist Pflicht

    out = TelemetryRecord{};                  // sauber zurücksetzen
    out.t_ms = static_cast<uint32_t>(std::strtoul(f[0].c_str(), nullptr, 10));

    // UTC (Feld [1]): "hh:mm:ss" -> Werte; leeres Feld -> has_utc bleibt false.
    if (!f[1].empty()) {
        unsigned h = 0, m = 0, sec = 0;
        if (std::sscanf(f[1].c_str(), "%u:%u:%u", &h, &m, &sec) == 3) {
            out.has_utc = true;
            out.utc_h = static_cast<uint8_t>(h);
            out.utc_min = static_cast<uint8_t>(m);
            out.utc_s = static_cast<uint8_t>(sec);
        }
    }

    // Phase aus Label (Feld [2]).
    const std::string& p = f[2];
    if      (p == "PREFLIGHT") out.phase = Phase::PreFlight;
    else if (p == "ASCENT")    out.phase = Phase::Ascent;
    else if (p == "DESCENT")   out.phase = Phase::Descent;
    else if (p == "LANDED")    out.phase = Phase::Landed;

    // GPS: Fix gilt als vorhanden, wenn lat/lon (Felder [3]/[4]) befüllt sind.
    if (!f[3].empty() && !f[4].empty()) {
        out.has_fix = true;
        out.lat = std::strtod(f[3].c_str(), nullptr);
        out.lon = std::strtod(f[4].c_str(), nullptr);
        if (!f[5].empty()) out.alt_gps_m = static_cast<float>(std::strtod(f[5].c_str(), nullptr));
        if (!f[6].empty()) out.sats = static_cast<uint8_t>(std::strtoul(f[6].c_str(), nullptr, 10));
    }
    return true;
}
```

Sicherstellen, dass `<cstdio>` includiert ist (für `snprintf`/`sscanf`) — steht bereits ganz oben in `record.cpp`.

- [ ] **Step 6: Tests laufen lassen — müssen bestehen (GREEN)**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && pio test -e native -f test_record`
Expected: PASS — alle Record-Tests grün (7 alte inkl. der zwei angepassten + 6 neue = 13).

- [ ] **Step 7: Gesamte native Suite laufen lassen**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && pio test -e native`
Expected: PASS — alle Test-Ordner grün (nichts anderes gebrochen).

- [ ] **Step 8: Checkpoint / Commit**

```bash
git add lib/telemetry/record.h lib/telemetry/record.cpp test/test_record/test_record.cpp
git commit -m "feat(record): utc-Spalte (hh:mm:ss) nach t_ms ins CSV-Format"
```

---

### Task 3: UTC in die Flug-Pipeline verdrahten

**Files:**
- Modify: `src/flight/main.cpp` (Record-Befüllung)

**Interfaces:**
- Consumes: `GpsFix` (mit `has_utc`, `utc_h/min/s`) aus Task 1, `TelemetryRecord` (mit denselben Feldern) aus Task 2.
- Produces: keine neue Schnittstelle. Rein mechanische Zuweisung.

> **Kein nativer Test:** Dies ist Board-Code (`<Arduino.h>`), nicht nativ testbar (🔶). Die Logik ist bereits durch Task 1+2 abgesichert; hier werden nur die Felder kopiert. Verifikation erfolgt später am Board (TODO-Testreihenfolge).

- [ ] **Step 1: UTC-Felder aus `GpsFix` in den Record kopieren**

In `src/flight/main.cpp`, dort wo der `TelemetryRecord` aus dem `GpsFix` befüllt wird (bei `rec.has_fix = fix.has_fix;` etc.), die vier UTC-Zuweisungen ergänzen:

```cpp
        rec.has_utc = fix.has_utc;
        rec.utc_h   = fix.utc_h;
        rec.utc_min = fix.utc_min;
        rec.utc_s   = fix.utc_s;
```

(Diese Zuweisungen gehören *außerhalb* der `if (fix.has_fix)`-Bedingung — UTC ist vom Fix entkoppelt.)

- [ ] **Step 2: Flight-Build kompilieren**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && pio run -e flight`
Expected: SUCCESS — kompiliert fehlerfrei (Laufzeit am Board noch unverifiziert, 🔶).

- [ ] **Step 3: Checkpoint / Commit**

```bash
git add src/flight/main.cpp
git commit -m "feat(flight): GPS-UTC in TelemetryRecord verdrahten"
```

---

### Task 4: Software-Flow-Doku aktualisieren

**Files:**
- Modify: `docs/software-flow.md` (§5 CSV-Record, §6 Landkarte)

**Interfaces:** keine (Doku).

> Pflicht laut CLAUDE.md: nach jeder Änderung am Software-Ablauf wird `docs/software-flow.md` am Ende der Arbeit aktualisiert.

- [ ] **Step 1: §5 (CSV-Record) um die utc-Spalte ergänzen**

Im Mermaid-Diagramm in §5 den `TelemetryRecord`-Knoten und die Beispiel-CSV-Zeile um `utc` erweitern, z. B.:
- Record-Feldliste: `{t_ms, utc, phase, has_fix, lat, lon, alt, sats}`
- Beispielzeile: `'12345,12:35:19,ASCENT,48.1,11.5,530.0,7'`

Im Fließtext einen Satz ergänzen: die `utc`-Spalte (absolute GPS-Zeit `hh:mm:ss`, entkoppelt von `has_fix`) steht neben `t_ms` und macht den `t_ms`-Rücksprung nach Reset harmlos.

- [ ] **Step 2: §6 (Landkarte) — Zeile für die UTC-Spalte**

In der Tabelle in §6 den Record-Eintrag als „✅ nativ getestet, inkl. utc-Spalte" kennzeichnen bzw. eine Zeile ergänzen:
`| GPS-UTC-Spalte (hh:mm:ss) | lib/telemetry/gga + record | ✅ nativ getestet |`

Den letzten Absatz von §6 (der die UTC-Spalte noch als „geplant/nächster Baustein" beschreibt) auf „umgesetzt" aktualisieren.

- [ ] **Step 3: Checkpoint / Commit**

```bash
git add docs/software-flow.md
git commit -m "docs: Software-Flow um GPS-UTC-Spalte aktualisiert"
```

---

## Zusammenfassung der Reihenfolge

1. **Task 1** — GGA parst UTC (5 neue Tests, RED→GREEN).
2. **Task 2** — utc-Spalte im CSV (2 Alt-Tests angepasst, 6 neue, RED→GREEN, volle Suite grün).
3. **Task 3** — Verdrahtung in `src/flight/main.cpp` (Flight-Build kompiliert).
4. **Task 4** — `docs/software-flow.md` aktualisiert.

Endzustand: `utc`-Spalte nativ vollständig abgesichert, Flight-Build grün, Doku aktuell. Board-Verifikation bleibt Teil der bestehenden TODO-Testreihenfolge.
