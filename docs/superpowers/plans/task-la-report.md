# Task-Report: LineAssembler (Task 1 + 2)

**Datum:** 2026-07-04  
**Ausführendes Werkzeug:** Claude Code (Sonnet 4.6)

---

## Erstellte / geänderte Dateien

| Datei | Aktion |
|---|---|
| `lib/telemetry/line_assembler.h` | neu erstellt |
| `lib/telemetry/line_assembler.cpp` | neu erstellt |
| `test/test_line_assembler/test_line_assembler.cpp` | neu erstellt |

---

## TDD-Ablauf

### Task 1

1. **Header** `lib/telemetry/line_assembler.h` geschrieben (exakt wie im Plan).
2. **Test** `test/test_line_assembler/test_line_assembler.cpp` mit `test_simple_line` geschrieben.
3. **RED verifiziert:** `pio test -e native` → `test_line_assembler ERRORED` wegen Linker-Fehler (undefined reference auf `LineAssembler::push` und Konstruktor). Richtiger Grund, erwartet.
4. **Minimale Implementierung** `lib/telemetry/line_assembler.cpp` geschrieben (exakt wie im Plan).
5. **GREEN verifiziert:** alle 32 Tests (inkl. Bestand) bestanden.

### Task 2

1. **Weitere Tests** in `test_line_assembler.cpp` ergänzt (6 zusätzliche Fälle + RUN_TEST-Einträge).
2. **Alle Tests GREEN:** alle 38 Tests bestanden, inkl. `test_overflow_then_recover`.

---

## Abweichungen vom Plan-Code

**Keine.** Header, Implementierung und alle Tests entsprechen 1:1 dem Plan. Keine Anpassungen an Testerwartungen oder Implementierungslogik notwendig.

---

## Overflow-Logik — Verifikation

Der knifflige Fall `test_overflow_then_recover` lief im ersten Durchgang durch:

- Bei `buf_.size() >= max_len_` (4. Zeichen in einen 4-Byte-Puffer) wird `overflow_ = true` und `buf_.clear()`.
- Nachfolgende Bytes bis `\n` werden durch `if (overflow_) return false;` verworfen.
- `\n` mit `overflow_ == true` setzt `overflow_ = false`, löscht Puffer, liefert false → überlange Zeile komplett verworfen.
- Nächste Zeile `OK\n` kommt sauber durch.

Verhalten entspricht exakt der Spezifikation in den Global Constraints.

---

## Finales Testergebnis

```
pio test -e native
```

```
Environment    Test                 Status    Duration
-------------  -------------------  --------  ------------
native         test_flight_phase    PASSED    00:00:00.709
native         test_record          PASSED    00:00:00.552
native         test_line_assembler  PASSED    00:00:00.857
native         test_gga             PASSED    00:00:00.571
native         test_ubx             PASSED    00:00:00.600
================= 38 test cases: 38 succeeded in 00:00:03.289 =================
```

**38 von 38 Tests grün.** Keine Regressionen.

---

## Bedenken

Keine. Der Code ist einfach, gut abgedeckt und entspricht dem Plan ohne Abweichungen.

---

## Minor-Fixes (M1, M2)

**Datum:** 2026-07-04

### M1 — Grenzfall-Test `test_exact_max_len_delivered`

In `test/test_line_assembler/test_line_assembler.cpp` wurde ein neuer Test ergänzt, der absichert, dass eine Zeile mit GENAU `max_len` Zeichen gefolgt von `\n` noch sauber geliefert wird (kein Off-by-one). Der Test nutzt `LineAssembler a(4)` und speist `"ABCD\n"` ein — erwartet wird eine fertige Zeile `"ABCD"`.

Der Test lief im ersten Durchgang sofort GRÜN: kein Off-by-one-Bug vorhanden. Die Overflow-Bedingung `buf_.size() >= max_len_` greift erst, wenn ein Zeichen ankommt, während der Puffer bereits voll ist — bei `ABCD\n` (4 Zeichen + `\n`) werden alle 4 Zeichen eingepuffert, und das `\n` löst die reguläre Lieferung aus.

### M2 — Defensiver Kommentar in `line_assembler.cpp`

In `lib/telemetry/line_assembler.cpp`, `if (overflow_)`-Zweig beim `\n`-Block: Das `buf_.clear()` wurde mit dem Kommentar `// defensiv; buf_ ist im Overflow bereits leer` versehen. Die Zeile selbst bleibt erhalten — sie schadet nicht und schützt vor zukünftigen Refactorings.

### Testergebnis

```
export PATH="$PATH:$HOME/.platformio/penv/bin" && pio test -e native
```

```
Environment    Test                 Status    Duration
-------------  -------------------  --------  ------------
native         test_flight_phase    PASSED    00:00:01.205
native         test_record          PASSED    00:00:00.672
native         test_line_assembler  PASSED    00:00:00.867
native         test_gga             PASSED    00:00:00.674
native         test_ubx             PASSED    00:00:00.665
================= 39 test cases: 39 succeeded in 00:00:04.084 =================
```

**39 von 39 Tests grün.** Kein Regression, neuer Test sofort grün.
