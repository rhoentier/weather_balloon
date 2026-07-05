# OLED-Sensorstatuszeile Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Der OLED-Boden-Check zeigt eine fünfte Zeile, die auf einen Blick anzeigt, welche der vier Sensoren (BMP280, MPU-6050, DS18B20, UV) ok sind.

**Architecture:** Reine Erweiterung von `DisplayState` um vier `bool`-Flags und von `status_lines()` um eine fünfte, angehängte Zeile mit Kurzcodes (`B:ok M:-- D:-- U:--`). Bleibt vollständig im bereits hardware-freien, nativ getesteten Baustein `lib/telemetry/display_status` — kein `<Arduino.h>`, kein Zugriff auf `src/flight`.

**Tech Stack:** C++17, Unity (PlatformIO native), keine neuen Abhängigkeiten.

## Global Constraints

- Sprache: Deutsch (Doku, Kommentare) — siehe `CLAUDE.md`.
- TDD verbindlich: erst Test schreiben, RED sehen (aus dem richtigen Grund), dann minimaler Code (GREEN).
- YAGNI: genau vier `bool`-Felder (`bmp_ok`, `mpu_ok`, `ds18b20_ok`, `uv_ok`), kein generisches Array/Struct.
- Hardware-frei: `lib/telemetry/display_status.h/.cpp` bleibt ohne `<Arduino.h>`, nativ mit `pio test -e native` testbar.
- Layout-Kontrakt: bestehende Indizes `[0] Titel, [1] GPS, [2] SD, [3] Phase` bleiben unverändert; neue Sensorzeile wird als `[4]` angehängt.
- Format der neuen Zeile exakt: `"B:ok M:ok D:ok U:ok"` bzw. `"--"` statt `"ok"` je nach Flag, mit genau einem Leerzeichen zwischen den vier Kürzel-Wert-Paaren, Reihenfolge fest BMP280→MPU-6050→DS18B20→UV.
- Scope dieser Iteration: **nur** `lib/telemetry/display_status` + dessen Test. Kein Update von `src/flight/main.cpp` oder `oled.cpp` (siehe Spec `docs/superpowers/specs/2026-07-04-oled-sensorstatus-design.md`, Abschnitt „Zweck & Grenze").
- Git ist in diesem Projekt **nicht vorhanden** (kein Repo) — Commit-Schritte sind optional/übersprungen; Implementer arbeiten direkt auf der Arbeitskopie ohne Commits.
- Build/Test-Kommando: `export PATH="$PATH:$HOME/.platformio/penv/bin" && pio test -e native`

---

### Task 1: `DisplayState` um vier Sensor-Flags erweitern + Sensorzeile in `status_lines()`

**Files:**
- Modify: `lib/telemetry/display_status.h`
- Modify: `lib/telemetry/display_status.cpp`
- Modify: `test/test_display_status/test_display_status.cpp`

**Interfaces:**
- Consumes: nichts Neues — `DisplayState`/`status_lines()` existieren bereits (siehe `lib/telemetry/display_status.h`).
- Produces: `DisplayState` mit vier zusätzlichen `bool`-Feldern `bmp_ok`, `mpu_ok`, `ds18b20_ok`, `uv_ok`; `status_lines()` liefert jetzt einen `std::vector<std::string>` der Größe 5 (Index 4 = Sensorzeile). Diese Erweiterung ist für spätere Aufrufer (`src/flight/main.cpp`) relevant, wird dort aber in dieser Iteration **nicht** verdrahtet.

- [ ] **Step 1: Bestehenden Layout-Test auf fünf Zeilen erweitern + neue Tests schreiben**

Modify `test/test_display_status/test_display_status.cpp`:

**(a)** Den bestehenden Test `test_layout_has_four_lines_in_order` ersetzen (Name bleibt, Prüfung wird auf fünf Zeilen erweitert):

```cpp
void test_layout_has_four_lines_in_order() {
    auto l = status_lines(base());
    TEST_ASSERT_EQUAL_INT(5, (int)l.size());
    TEST_ASSERT_EQUAL_STRING("Wetterballon", l[0].c_str());
    TEST_ASSERT_TRUE(l[1].rfind("GPS:", 0) == 0);
    TEST_ASSERT_TRUE(l[2].rfind("SD:", 0) == 0);
    TEST_ASSERT_TRUE(l[3].rfind("Phase:", 0) == 0);
    TEST_ASSERT_TRUE(l[4].rfind("B:", 0) == 0);
}
```

**(b)** Die Hilfsfunktion `base()` um die vier neuen Felder ergänzen (alle initial `false`, wie bisher bei `sd_ok`):

```cpp
static DisplayState base() {
    return DisplayState{ GpsDisp::Silent, 0, false, Phase::PreFlight,
                          false, false, false, false };
}
```

**(c)** Neue Tests vor `main()` einfügen:

```cpp
// Alle vier Sensoren ok -> alle vier Kürzel zeigen "ok".
void test_sensors_all_ok() {
    auto s = base();
    s.bmp_ok = true; s.mpu_ok = true; s.ds18b20_ok = true; s.uv_ok = true;
    TEST_ASSERT_EQUAL_STRING("B:ok M:ok D:ok U:ok", status_lines(s)[4].c_str());
}

// Kein Sensor ok -> alle vier Kürzel zeigen "--".
void test_sensors_all_missing() {
    auto s = base();
    TEST_ASSERT_EQUAL_STRING("B:-- M:-- D:-- U:--", status_lines(s)[4].c_str());
}

// Gemischt: nur BMP280 ok -> die anderen drei bleiben unabhängig "--".
void test_sensors_mixed() {
    auto s = base();
    s.bmp_ok = true;  // mpu_ok, ds18b20_ok, uv_ok bleiben false
    TEST_ASSERT_EQUAL_STRING("B:ok M:-- D:-- U:--", status_lines(s)[4].c_str());
}
```

In `main()` ergänzen (nach den bestehenden `RUN_TEST`-Aufrufen, vor `return UNITY_END();`):

```cpp
    RUN_TEST(test_sensors_all_ok);
    RUN_TEST(test_sensors_all_missing);
    RUN_TEST(test_sensors_mixed);
```

- [ ] **Step 2: Tests laufen lassen und Fehlschlag verifizieren**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && pio test -e native -f test_display_status`
Expected: FAIL — Kompilierfehler, da `DisplayState{...}` mit 8 statt 4 Argumenten aufgerufen wird (`bmp_ok`/`mpu_ok`/`ds18b20_ok`/`uv_ok` sind noch keine Member) und `status_lines()` noch keinen Index `[4]` liefert.

- [ ] **Step 3: `DisplayState` um vier Felder erweitern**

Modify `lib/telemetry/display_status.h` — den Struct ersetzen durch:

```cpp
struct DisplayState {
    GpsDisp gps;
    uint8_t sats;
    bool    sd_ok;
    Phase   phase;
    bool    bmp_ok;      // BMP280 ok?
    bool    mpu_ok;      // MPU-6050 ok?
    bool    ds18b20_ok;  // DS18B20 ok?
    bool    uv_ok;       // GUVA-S12SD (UV) ok?
};

// Anzuzeigende Zeilen. Reihenfolge = Layout-Kontrakt:
//   [0] Titel, [1] GPS, [2] SD, [3] Phase, [4] Sensoren.
std::vector<std::string> status_lines(const DisplayState& s);
```

- [ ] **Step 4: Sensorzeile in `status_lines()` bauen**

Modify `lib/telemetry/display_status.cpp` — ersetze die Funktion durch:

```cpp
std::vector<std::string> status_lines(const DisplayState& s) {
    std::string gps;
    switch (s.gps) {
        case GpsDisp::Silent:  gps = "GPS: --";       break;
        case GpsDisp::Waiting: gps = "GPS: warte...";  break;
        case GpsDisp::Fix:     gps = "GPS: Fix " + std::to_string((int)s.sats) + " Sat"; break;
    }
    std::string sensors = std::string("B:") + (s.bmp_ok     ? "ok" : "--")
                         + " M:"             + (s.mpu_ok     ? "ok" : "--")
                         + " D:"             + (s.ds18b20_ok ? "ok" : "--")
                         + " U:"             + (s.uv_ok      ? "ok" : "--");
    return {
        "Wetterballon",
        gps,
        s.sd_ok ? "SD: ok" : "SD: --",
        std::string("Phase: ") + to_string(s.phase),
        sensors,
    };
}
```

- [ ] **Step 5: Tests laufen lassen und Erfolg verifizieren**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && pio test -e native -f test_display_status`
Expected: PASS — alle Tests grün (6 alte, davon 1 angepasst + 3 neue = 9 Tests).

- [ ] **Step 6: Gesamte native Suite laufen lassen**

Run: `export PATH="$PATH:$HOME/.platformio/penv/bin" && pio test -e native`
Expected: PASS — alle Test-Ordner grün (bestehende 70 Tests, davon `test_display_status` jetzt mit 9 statt 6 Tests → 73 Tests gesamt).

- [ ] **Step 7: Commit (optional, nur falls Git-Repo)**

```bash
git rev-parse --is-inside-work-tree >/dev/null 2>&1 && \
  git add lib/telemetry/display_status.h lib/telemetry/display_status.cpp test/test_display_status/test_display_status.cpp && \
  git commit -m "feat(display_status): Sensorstatuszeile (BMP280/MPU-6050/DS18B20/UV)"
```

---

### Task 2: Software-Flow-Doku aktualisieren

**Files:**
- Modify: `docs/software-flow.md`

**Interfaces:**
- Consumes: den in Task 1 fertiggestellten `lib/telemetry/display_status`-Baustein (fünf Zeilen statt vier).
- Produces: nichts für spätere Tasks — reine Dokumentationsaktualisierung.

- [ ] **Step 1: Absatz zum OLED-Boden-Check in §3 um die Sensorzeile ergänzen**

Modify `docs/software-flow.md` — im Absatz, der mit `**OLED-Boden-Check (`status_lines()` + `oled_*`):**` beginnt (aktuell Zeile 181), den Satz über die vier Zeilen ersetzen:

Alt:
```
Solange die Phase `PreFlight` ist, zeigt das
Display vier Zeilen (Titel, GPS-Stufe inkl. Satellitenzahl, SD-Status, Phase) —
der „Startklar?"-Check ohne Laptop am Startort.
```

Neu:
```
Solange die Phase `PreFlight` ist, zeigt das
Display fünf Zeilen (Titel, GPS-Stufe inkl. Satellitenzahl, SD-Status, Phase,
Sensorstatus) — der „Startklar?"-Check ohne Laptop am Startort. Die fünfte
Zeile fasst die vier Sensoren als Kurzcodes zusammen (`B:ok M:-- D:-- U:--` für
BMP280/MPU-6050/DS18B20/UV) — `ok`, wenn das jeweilige `*_ok`-Flag im
`DisplayState` gesetzt ist, sonst `--`. Aktuell befüllt `src/flight/main.cpp`
diese vier Flags noch **nicht** (⬜) — das ist ein späterer Schritt, sobald die
jeweilige Sensor-Anbindung steht.
```

- [ ] **Step 2: Neue Erklärungs-Box in §5 (analog zu `fix_q`/`utc`/`has_bmp`) ergänzen**

Modify `docs/software-flow.md` — nach dem Absatz `**Warum ein gemeinsames \`has_bmp\`-Flag für drei Spalten?**` (endet aktuell bei Zeile 295 mit `aufgerufen von \`src/flight\`.`) folgenden neuen Absatz einfügen:

```markdown

**Warum vier einzelne `*_ok`-Flags in `DisplayState` statt eines CSV-Felds?**
Der Sensorstatus in der OLED-Zeile ist reine **Boden-Diagnose** („ist der
Sensor gerade ansprechbar?"), keine Telemetrie fürs CSV-Log — er gehört daher
zu `lib/telemetry/display_status`, nicht zu `record`. Vier einzelne `bool`s
(analog zu `sd_ok`) statt eines generischen Arrays, weil es bei genau vier
fest benannten Sensoren bleibt (YAGNI).
```

- [ ] **Step 3: Landkarten-Tabelle in §6 um eine Zeile ergänzen**

Modify `docs/software-flow.md` — in der Tabelle (aktuell ab Zeile 301) die Zeile

```
| OLED-Boden-Check: Zeilen-Formatierung | `lib/telemetry/display_status` | ✅ nativ getestet |
```

ersetzen durch:

```
| OLED-Boden-Check: Zeilen-Formatierung (inkl. Sensorstatuszeile) | `lib/telemetry/display_status` | ✅ nativ getestet |
| OLED-Sensorstatus: main.cpp befüllt `*_ok`-Flags | `src/flight/main.cpp` | ⬜ |
```

- [ ] **Step 4: Schluss-Prosa um einen Absatz zur Sensorstatuszeile ergänzen**

Modify `docs/software-flow.md` — am Ende der Datei (nach dem letzten Absatz „Der nächste offene, rein testbare `lib`-Baustein sind die **MPU-6050-Rohwert-Umrechnungen** (→ °/s und g)." ) folgenden Absatz anhängen:

```markdown

Der **OLED-Boden-Check** zeigt jetzt zusätzlich eine **Sensorstatuszeile**
(`lib/telemetry/display_status`, fertig und nativ getestet, 9 Tests grün):
`DisplayState` trägt vier unabhängige `bool`-Flags (`bmp_ok`, `mpu_ok`,
`ds18b20_ok`, `uv_ok`), `status_lines()` fasst sie als fünfte Zeile in
Kurzcodes zusammen (`B:ok M:-- D:-- U:--`) — siehe
`docs/superpowers/specs/2026-07-04-oled-sensorstatus-design.md`. Offen ist das
Befüllen dieser vier Flags in `src/flight/main.cpp`: `bmp_ok` könnte aus
`g_rec.has_bmp` abgeleitet werden, sobald die BMP280-I²C-Anbindung steht;
`mpu_ok`/`ds18b20_ok`/`uv_ok` bleiben fest `false`, bis die jeweiligen Sensoren
überhaupt gelesen werden (siehe §6, MPU-6050/DS18B20/UV weiterhin ⬜).
```

- [ ] **Step 5: Commit (optional, nur falls Git-Repo)**

```bash
git rev-parse --is-inside-work-tree >/dev/null 2>&1 && \
  git add docs/software-flow.md && \
  git commit -m "docs(software-flow): OLED-Sensorstatuszeile dokumentieren"
```

---

## Self-Review

**1. Spec coverage:** Alle Abschnitte der Spec (`docs/superpowers/specs/2026-07-04-oled-sensorstatus-design.md`) sind abgedeckt:
- Interface-Erweiterung `DisplayState`/`status_lines()` → Task 1, Step 3–4.
- Design-Entscheidung „vier einzelne bools" → Task 1, Step 3 (exakt wie Spec).
- Design-Entscheidung „neue Zeile angehängt, Indizes 0–3 unverändert" → Task 1, Step 1a (Assertions für `l[0]..l[3]` unverändert übernommen) + Step 4.
- Design-Entscheidung „Kurzcodes wegen 21-Zeichen-Budget" → Task 1, Step 4 (exaktes Format `B:ok M:-- D:-- U:--`).
- Testreihenfolge (4 Punkte aus der Spec: Layout-Erweiterung, alle ok, alle fehlend, gemischt) → Task 1, Step 1a–c, deckungsgleich.
- „Nach der Umsetzung: docs/software-flow.md aktualisieren" → Task 2, vollständig.
- Ausdrücklich außerhalb des Scopes (`src/flight` nicht anfassen) → in Global Constraints und Task-2-Doku-Text explizit erwähnt, keine Code-Task berührt `src/flight`.

**2. Placeholder scan:** Kein „TBD"/„TODO"/offen gelassene Stellen; jeder Schritt enthält vollständigen Code bzw. exakten Such-/Ersatztext für die Doku-Änderungen.

**3. Type consistency:** `DisplayState`-Feldnamen (`bmp_ok`, `mpu_ok`, `ds18b20_ok`, `uv_ok`) sind in Spec, Task-1-Header-Änderung, `.cpp`-Änderung und allen drei neuen Tests identisch geschrieben. `status_lines()`-Signatur unverändert (`std::vector<std::string> status_lines(const DisplayState&)`). Die in Task 2 zitierten Codezeilen (`docs/software-flow.md`) wurden gegen den aktuell gelesenen Dateiinhalt abgeglichen (inkl. der beim OLED-Timing-Update bereits geänderten Absatz-Formulierung „Der Display-Block läuft am Ende jeder loop()-Iteration...").
