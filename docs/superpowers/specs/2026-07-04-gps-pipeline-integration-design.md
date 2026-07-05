# Design: GPS-Pipeline-Integration (loop + LineAssembler + SD-Logging)

Datum: 2026-07-04
Status: abgesegnet, bereit für Implementierungsplan

## Ziel

Die bisher isoliert getesteten Logik-Bausteine (`parse_gga`, `TelemetryRecord`,
`FlightPhaseDetector`) erstmals zu einer laufenden Pipeline auf dem Flug-Board
verdrahten. Ergebnis: `loop()` liest den GPS-UART-Strom, baut daraus
Telemetrie-CSV-Zeilen und schreibt sie auf **Serial (USB)** und **microSD**.

Damit wird die GPS-Kette erstmals End-to-End am echten NEO-6M sichtbar — der
erste Punkt der TODO-Testreihenfolge — und macht Verkabelungsfehler früh
auffindbar.

## Datenfluss (pro loop-Iteration)

```
Serial2 (UART2)  →  LineAssembler  →  parse_gga  →  TelemetryRecord  →  FlightPhaseDetector  →  csv_row  →  Serial + microSD
   Bytes             fertige Zeile     GpsFix         befüllen            Phase                  CSV-Zeile
```

## Architektur-Einordnung

Der einzige nicht-triviale, testbare Kern (das Zusammensetzen der UART-Bytes zu
Zeilen) wird **hardware-frei** in `lib/telemetry/` gebaut und nativ getestet.
Alles andere ist zwangsläufig Hardware-gebunden (`Serial2`, `SD`, `millis()`)
und lebt in `src/flight/` — geschrieben, aber am Board zu verifizieren (🔶).

---

## Komponente 1 — `LineAssembler` (`lib/telemetry/line_assembler.h/.cpp`)

Hardware-frei, reines C++17, Namespace `telemetry`. Nimmt den UART-Bytestrom
Byte für Byte entgegen und liefert komplette Zeilen.

```cpp
#include <cstddef>
#include <string>

namespace telemetry {

class LineAssembler {
public:
    explicit LineAssembler(std::size_t max_len = 120);

    // Ein Byte einspeisen. Rückgabe true = eine vollständige, nicht-leere Zeile
    // ist fertig und steht in out_line (ohne \r und \n). Sonst false.
    bool push(char c, std::string& out_line);

    // Puffer verwerfen (z.B. nach einem Fehler).
    void reset();

private:
    std::string buf_;
    std::size_t max_len_;
    bool overflow_ = false;  // true = aktuelle Zeile war zu lang, bis \n verwerfen
};

} // namespace telemetry
```

### Verhalten (exakt)

- **`\n`** beendet die Zeile: aktueller Puffer wird als `out_line` geliefert
  (Rückgabe `true`), Puffer geleert. **Leere Zeile** (`\n` ohne Inhalt) → `false`
  (keine leere Zeile durchreichen).
- **`\r`** wird immer verworfen (CRLF-sicher — NMEA endet mit `\r\n`).
- **Overflow-Schutz:** Erreicht der Puffer `max_len` Zeichen ohne `\n`, gilt die
  Zeile als defekt: Puffer wird verworfen und bis zum nächsten `\n` neu
  synchronisiert (die überlange Zeile wird komplett verworfen, kein Teilstück
  geliefert). So wächst der Puffer auf dem ESP32 nie unbegrenzt.
- **`max_len` = 120** als Default: eine typische NMEA-Zeile ist < 82 Zeichen;
  120 gibt Reserve, ohne viel RAM zu binden.

### Native Tests (`test/test_line_assembler/`, TDD)

RED zuerst, gegen konkrete Byte-Folgen:
1. **Eine einfache Zeile:** `"ABC\n"` byteweise → einmal `true` mit `"ABC"`.
2. **Fragmentiert:** dieselbe Zeile in mehreren `push`-Aufrufen — Ergebnis identisch.
3. **CRLF:** `"ABC\r\n"` → `out_line == "ABC"` (kein `\r`).
4. **Mehrere Zeilen am Stück:** `"A\nB\n"` → zweimal `true` mit `"A"` bzw. `"B"`.
5. **Leere Zeile:** `"\n"` → `false` (nichts geliefert).
6. **Overflow:** Zeile länger als `max_len` ohne `\n`, dann `\n`, dann eine
   gültige Zeile → die überlange wird verworfen, die nächste gültige kommt sauber.
7. **`reset()`** leert einen halb gefüllten Puffer (danach beginnt die nächste
   Zeile frisch).

---

## Komponente 2 — `loop()`-Verdrahtung (`src/flight/main.cpp`)

Dünne Board-Logik, kein nativ testbarer Teil. Statisch in der Datei leben ein
`telemetry::LineAssembler`, ein `telemetry::TelemetryRecord` und ein
`telemetry::FlightPhaseDetector`.

Ablauf (Pseudocode):
```
loop():
  while (Serial2.available()):
    char c = Serial2.read()
    std::string line
    if (asm.push(c, line)):
      GpsFix fix
      if (parse_gga(line, fix)):
        rec.t_ms = millis()                 // monotone Bordzeit
        rec.has_fix   = fix.has_fix
        rec.lat       = fix.lat
        rec.lon       = fix.lon
        rec.alt_gps_m = fix.alt_gps_m
        rec.sats      = fix.sats
        if (fix.has_fix):
          rec.phase = detector.update(fix.alt_gps_m, rec.t_ms)
        else:
          rec.phase = detector.phase()      // letzte bekannte Phase halten
        String csv = csv_row(rec).c_str()
        Serial.println(csv)
        sd_log(csv)
```

**Warum Phase nur bei Fix aktualisieren?** Ohne Fix gibt es keine Höhe → keine
sinnvolle Steig-/Sinkrate. Ein einzelner Fix-Aussetzer darf die Phase nicht
verfälschen; deshalb wird bei fehlendem Fix die zuletzt erkannte Phase gehalten.

**Warum nur GGA-Zeilen etwas auslösen?** `parse_gga` gibt für alle Nicht-GGA-
Sätze (RMC, GSA, …) `false` zurück — die werden stillschweigend übersprungen.
Ein Record entsteht also pro GGA-Satz (beim NEO-6M im Sekundentakt).

`t_ms = millis()` bezieht sich auf die Bordzeit seit Boot. **Bekannter
Nebeneffekt:** Bei einem Neustart beginnt `millis()` wieder bei 0, d.h. in einer
über mehrere Läufe fortgeschriebenen Datei springt `t_ms` zurück. Das ist bei
der Auswertung als Neustart-Marker erkennbar und für dieses Projekt akzeptabel.
Siehe Abschnitt „Zeit & Synchronisierung" für den geplanten Ausbau.

## Zeit & Synchronisierung

**Entscheidung für diesen Schritt:** `t_ms` = `millis()` (monotone Bordzeit seit
Boot). Das ist die einzige Zeit, die *vor* dem ersten GPS-Fix und *ohne*
zusätzliches Parsing verfügbar ist, und für Reihenfolge und Steig-/Sinkraten
(→ Flugphase) völlig ausreichend. `millis()` ist monoton und fein aufgelöst,
aber **relativ** (Boot = 0) und driftet leicht — als *absoluter* Zeitstempel
taugt es nicht, und beim Neustart springt es zurück.

**Geplanter nächster Baustein (eigene Spec, NICHT Teil dieses Schritts):** eine
**UTC-Spalte** aus dem GPS. Das GPS führt eine atomuhrgenaue UTC mit; GGA liefert
davon die **Uhrzeit** (`hhmmss.ss`, kein Datum — das Datum käme aus dem
Flugprotokoll oder später aus RMC). Aufgenommen wird sie YAGNI-konform über die
bekannte 3-Stellen-Regel des Record-Formats, sobald sie tatsächlich gelesen
wird. In der Auswertung stehen dann `t_ms` (monoton, fein) **und** `utc`
(absolut, neustartfest) nebeneinander — der `t_ms`-Rücksprung nach einem Reset
wird damit harmlos, weil die absolute Zeit daneben steht.

Ein externes RTC-Modul (z. B. DS3231) wäre **redundant** zum ohnehin an Bord
befindlichen GPS und wird bewusst nicht verwendet (Gewicht/Kosten, YAGNI).

---

## Komponente 3 — microSD-Logging (`src/flight/sd_log.h/.cpp`)

Dünner Wrapper um die Arduino-`SD`-Bibliothek (SPI, CS an GPIO13 lt. Pin-Spec,
Bus mit LoRa geteilt).

```cpp
#include <Arduino.h>

// Initialisiert die SD-Karte (SPI, CS13). Rückgabe: true = Karte bereit.
// Legt bei neuer/leerer Datei die CSV-Kopfzeile an.
bool sd_log_begin();

// Hängt eine CSV-Zeile (ohne Zeilenende) an die Log-Datei an.
// Bei nicht initialisierter/fehlerhafter Karte: no-op (Betrieb läuft weiter).
void sd_log(const String& line);
```

### Verhalten

- **Feste Datei `/flight.csv`, Append-Modus.** Bei Neustart wird weiter-
  geschrieben (kein Überschreiben, keine neue Datei pro Boot). So bleibt ein
  Flug auch nach einem Reset in einer Datei zusammenhängend.
- **Header nur einmal:** In `sd_log_begin()` wird geprüft, ob `/flight.csv`
  existiert und nicht leer ist. Nur wenn sie neu/leer ist, wird `csv_header()`
  als erste Zeile geschrieben. So entsteht bei Append kein doppelter Header.
- **Robustheit:** Schlägt `SD.begin(PIN_SD_CS)` fehl, gibt `sd_log_begin()`
  `false` zurück und `sd_log()` wird zum no-op. Es wird **einmal** eine Warnung
  auf Serial ausgegeben. Der Flug darf nicht an einer fehlenden SD-Karte
  scheitern — das Serial-Log bleibt in jedem Fall erhalten.

**Offener Hardware-Punkt (aus TODO):** SD-Versorgung 3,3 V vs. 5 V ist noch nicht
verifiziert. Das ist ein Punkt der Board-Testreihenfolge und klärt sich mit
diesem Integrationstest — es blockiert die Implementierung des Codes nicht.

---

## setup()-Ergänzung

Nach der bestehenden GPS-Flight-Mode-Initialisierung wird `sd_log_begin()`
aufgerufen und das Ergebnis auf Serial protokolliert (bereit / nicht gefunden).

---

## Testbarkeit & Grenzen

- **Nativ getestet:** nur `LineAssembler` (Komponente 1). Das ist der einzige
  Teil mit nicht-trivialer Logik und ohne Hardware-Abhängigkeit.
- **Board-Verifikation (🔶, nächster Hardware-Termin):**
  - Kommen plausible CSV-Zeilen über Serial (Header + Datenzeilen, korrekte
    Spaltenzahl, sinnvolle Koordinaten)?
  - Wird `/flight.csv` mit genau einer Kopfzeile geschrieben und bei Neustart
    ohne zweiten Header fortgesetzt?
  - Verhält sich der Code ohne SD-Karte gutmütig (Warnung + Weiterbetrieb)?
  - SD-Versorgung 3,3/5 V klären (TODO-Punkt).

## Nicht-Ziele (YAGNI)

- Kein LoRa (eigener ⬜-Baustein).
- Keine Sensoren (BMP280/MPU/DS18B20/UV — noch ⬜).
- Kein Rate-Limiting / Bergungsmodus (eigener ⬜-Baustein; die Phase-Erkennung
  liefert bereits `Landed`, aber es wird noch nicht darauf reagiert).
- Keine neue Datei pro Boot, keine Dateirotation.
- Kein NMEA-Satz außer GGA (RMC/GSA/GSV werden übersprungen).

## Software-Flow nachpflegen

Nach der Implementierung `docs/software-flow.md` aktualisieren:
- §3 loop()-Diagramm: Knoten `READ_SENS` bleibt ⬜; `BUILD`, `SD` und der
  Zusammenbau werden auf den neuen Stand gebracht (LineAssembler + parse_gga +
  Record + Phase + csv_row + SD-Schreiben als 🔶 „geschrieben, Board-Test offen").
- §6-Tabelle: `LineAssembler` als neuer ✅-Baustein; microSD-Logging von ⬜ auf
  🔶.
