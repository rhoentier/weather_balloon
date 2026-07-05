# GPS-UTC-Spalte — Design

**Datum:** 2026-07-04
**Status:** freigegeben (Brainstorming abgeschlossen)
**Betrifft:** `lib/telemetry/gga`, `lib/telemetry/record`, Aufrufer `src/flight/main.cpp`

---

## 1. Ziel & Warum

Jede CSV-Zeile trägt bisher nur `t_ms` (= `millis()`, monotone Bordzeit seit
Boot). Das ist relativ und springt bei jedem Neustart auf 0 zurück — als
*absoluter* Zeitstempel taugt es nicht.

Das GPS führt ohnehin eine atomuhrgenaue UTC mit; die GGA-Zeile liefert davon die
**Uhrzeit** (`hhmmss(.ss)`, kein Datum). Wir nehmen sie als zusätzliche Spalte
`utc` ins Telemetrie-/CSV-Format auf. In der Auswertung stehen dann `t_ms`
(monoton, fein) **und** `utc` (absolut, neustartfest) nebeneinander — das macht
den `t_ms`-Rücksprung nach einem Reset harmlos und erlaubt den Abgleich von
SD-Log ↔ Empfangs-Log ↔ Action-Cam-Video über die Wanduhr-Zeit.

Dieser Baustein ist **hardware-frei** und rein nativ testbar (kein Board nötig).

## 2. Architektur & Aufteilung

Zwei bestehende `lib`-Module werden erweitert — kein neuer Baustein, keine neue
Datei:

1. **`lib/telemetry/gga`** — parst bereits die GGA-Zeile, ignoriert bislang aber
   Feld `[1]` (UTC). Wir erweitern `GpsFix` um die Uhrzeit als **Rohwerte**
   (ganze Zahlen). Der Parser bleibt „GPS-only" und kennt kein CSV.

2. **`lib/telemetry/record`** — bekommt die neue Spalte nach der bekannten
   **3-Stellen-Regel** (Feld im Record, `csv_header()`, `csv_row()` +
   `parse_csv_row()`). Die **Formatierung** `hh:mm:ss` lebt hier (CSV-Schicht),
   nicht im Parser.

Der Aufrufer in `src/flight/main.cpp` kopiert die UTC-Felder aus `GpsFix` in den
`TelemetryRecord` — genau wie schon lat/lon/alt/sats.

## 3. Datenrepräsentation

### In `GpsFix` (gga.h) — Rohwerte, keine Formatierung
```cpp
bool    has_utc = false;   // true = gültige UTC aus GGA-Feld [1] geparst
uint8_t utc_h   = 0;       // 0..23
uint8_t utc_min = 0;       // 0..59
uint8_t utc_s   = 0;       // 0..59  (Nachkommastellen verworfen)
```

### In `TelemetryRecord` (record.h) — dieselben vier Felder
`has_utc`, `utc_h`, `utc_min`, `utc_s`.

### Im CSV (record.cpp)
- **Header:** `t_ms,utc,phase,lat,lon,alt_gps_m,sats` — `utc` direkt nach `t_ms`,
  damit die beiden Zeit-Spalten (monoton + absolut) nebeneinander stehen.
- **`csv_row()`:** bei `has_utc` → `hh:mm:ss` mit führenden Nullen
  (`snprintf("%02u:%02u:%02u")`, z. B. `08:05:09`); sonst **leeres Feld**.
- **`parse_csv_row()`:** Feld an `:` splitten, drei Teile → `has_utc = true`;
  leeres Feld → `has_utc = false`. Spaltenzahl-Prüfung steigt von `6` auf `7`.

## 4. Parsing-Logik (gga.cpp)

GGA-Feld `[1]` ist `hhmmss` oder `hhmmss.ss`. Gültig, wenn **mindestens 6
Ziffern** vorn stehen. Wir schneiden `hh`/`mm`/`ss` aus den ersten 6 Zeichen,
verwerfen alles ab dem `.`. Leeres Feld oder < 6 Zeichen → `has_utc = false`,
Felder bleiben 0.

**Kern-Entscheidung — `has_utc` ist von `has_fix` entkoppelt:** Ein NEO-6M
sendet oft schon eine gültige Uhrzeit, *bevor* er einen vollen Positions-Fix
hat. Die UTC wird darum gelesen, sobald GGA sie liefert — unabhängig von der
Fix-Qualität. Zeit und Position sind zwei getrennte Konzepte.

## 5. Grenzfälle (bewusst behandelt)

- **Führende Nullen:** `08:05:09` — sonst bricht der Round-Trip bei einstelligen
  Werten.
- **Leeres Feld ↔ `has_utc=false`** sauber in beide Richtungen (Round-Trip).
- **Nachkommastellen** in der GGA-Zeit werden still verworfen.
- **UTC ohne Fix:** gültige Zeit + Fix-Qualität 0 → `has_utc=true`,
  `has_fix=false`.

## 6. Bewusst NICHT dabei (YAGNI)

- **Kein Datum** — GGA hat keins; käme später aus RMC oder dem Flugprotokoll.
- **Keine Zeitzone** — immer UTC.
- **Keine Sekundenbruchteile** — bei ~1 Hz Logging Overkill.
- **Kein Mitternacht-Sonderfall** — `hh:mm:ss` ist eindeutig; Tageswechsel
  braucht keine Behandlung.
- **Kein externes RTC** (DS3231) — redundant zum GPS an Bord (Gewicht/Kosten).

## 7. Tests (TDD, RED zuerst)

### `test/test_gga/` — UTC aus GGA parsen
1. **Normale Zeit:** GGA mit `123519.00` → `has_utc=true`, `12/35/19`.
2. **Führende Nullen:** `080509.00` → `8/5/9`.
3. **Ohne Nachkommastellen:** `123519` → identisch zu Fall 1.
4. **UTC ohne Positions-Fix:** gültige Zeit, Fix-Qualität `0` → `has_utc=true`,
   `has_fix=false` (Kern der Entkopplung).
5. **Leeres UTC-Feld:** GGA mit leerem Feld [1] → `has_utc=false`.
6. **Zu kurz / kaputt:** `1235` (< 6 Ziffern) → `has_utc=false`.

### `test/test_record/` — CSV-Format & Round-Trip
7. **Header:** `utc` an zweiter Stelle (`t_ms,utc,phase,...`).
8. **Row mit UTC:** Record `8/5/9` → Zeile enthält `08:05:09`.
9. **Row ohne UTC:** `has_utc=false` → leeres Feld an Position 2.
10. **Round-Trip mit UTC:** Record → `csv_row` → `parse_csv_row` → identische
    UTC-Felder + `has_utc=true`.
11. **Round-Trip ohne UTC:** leeres Feld → `has_utc=false` zurück.
12. **Spaltenzahl:** Zeile mit 6 statt 7 Spalten → `parse_csv_row` = `false`.

Die **bestehenden** Round-Trip-Tests werden auf 7 Spalten mitgezogen.
Ablauf pro Test: erst RED (aus dem richtigen Grund fehlschlagen sehen), dann
minimaler GREEN-Code, dann aufräumen.

## 8. Auswirkungen auf andere Dateien

- **`src/flight/main.cpp`:** vier Zeilen mehr beim Record-Befüllen (UTC-Felder
  aus `GpsFix` kopieren). 🔶 Board-unverifiziert wie der Rest der Pipeline.
- **`docs/software-flow.md`:** §5 (CSV-Record) und die Landkarte §6 nach der
  Umsetzung aktualisieren.
