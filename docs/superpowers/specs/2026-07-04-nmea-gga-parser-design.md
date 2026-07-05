# Design: NMEA-GGA-Parser (`lib/telemetry/gga`)

Datum: 2026-07-04
Status: abgesegnet, bereit für Implementierungsplan

## Zweck & Grenze

Ein **hardware-freier** Logik-Baustein, der *eine* NMEA-Zeile (String) in einen
`GpsFix` verwandelt. Er schließt die Lücke im `loop()`-Ablauf zwischen „GPS
sendet" (Flight-Mode ist gesetzt) und „`TelemetryRecord` wird befüllt".

Der Parser kennt **nur GPS**, nicht das Telemetrie-/CSV-Format. Damit bleibt die
Verantwortlichkeit sauber getrennt: `src/flight/main.cpp` liest UART-Zeilen vom
NEO-6M, ruft `parse_gga()` und kopiert bei Erfolg die Felder in den
`TelemetryRecord`. Der Parser selbst läuft als nativer Unit-Test auf dem Mac in
Millisekunden (reines C++17, kein `<Arduino.h>`).

## Warum nur GGA?

Der NEO-6M sendet mehrere NMEA-Satztypen im Sekundentakt. `$..GGA` (Global
Positioning System Fix Data) enthält bereits **alles**, was der `TelemetryRecord`
an GPS-Daten braucht: Fix-Status, Breite, Länge, Höhe über Meer, Satellitenzahl.
YAGNI: kein zweiter Satztyp, kein Kombinieren mehrerer Sätze. Datum/Zeit (RMC)
oder Fix-Qualitätsdetails (GSA/GSV) sind aktuell nicht im Record-Format
vorgesehen und werden erst gebaut, wenn sie gebraucht werden.

## Interface (`lib/telemetry/gga.h`)

```cpp
#include <cstdint>
#include <string>

struct GpsFix {
    bool     has_fix;      // true = gültiger Satellitenfix (Fix-Qualität > 0)
    double   lat, lon;     // Dezimalgrad; N/O positiv, S/W negativ
    float    alt_gps_m;    // Höhe über Meeresspiegel (GGA-Feld, "M")
    uint8_t  sats;         // Anzahl genutzter Satelliten
};

// return false -> keine brauchbare GGA-Zeile (kein $..GGA, kaputte Checksumme,
//                 Formfehler). out bleibt UNBERÜHRT.
// return true  -> wohlgeformter GGA-Satz mit gültiger Checksumme.
//                 out.has_fix = (Fix-Qualität > 0).
//                 Ohne Fix sind lat/lon/alt/sats undefiniert -> Aufrufer
//                 schreibt dann leere CSV-Felder (siehe record.h).
bool parse_gga(const std::string& line, GpsFix& out);
```

### Rückgabe-Semantik (bewusst gewählt)

`bool` trennt „Zeile unbrauchbar" von „gültige Zeile, aber noch kein Fix":

| Fall | Rückgabe | `out.has_fix` |
|---|---|---|
| Kein GGA-Satz (z.B. `$GPRMC…`) | `false` | — (out unberührt) |
| GGA, aber Checksumme falsch | `false` | — (out unberührt) |
| GGA, Formfehler / abgeschnitten | `false` | — (out unberührt) |
| GGA gültig, Fix-Qualität `0` | `true` | `false` |
| GGA gültig, Fix-Qualität > 0 | `true` | `true` |

Das spiegelt bewusst `parse_csv_row()` (ebenfalls `bool`). Der Aufrufer fragt
zuerst „war die Zeile überhaupt brauchbar?" und dann „hatten wir einen Fix?".

## GGA-Feldlayout (Referenz)

```
$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47
   |     |       |      |     |     | | |   |    |   |  |
   |     |       |      |     |     | | |   |    |   |  +-- Geoid-Trennung Einheit
   |     |       |      |     |     | | |   |    |   +----- Geoid-Trennung
   |     |       |      |     |     | | |   |    +--------- Höhen-Einheit "M"
   |     |       |      |     |     | | |   +-------------- Höhe über Meer  -> alt_gps_m
   |     |       |      |     |     | | +------------------ HDOP (ignoriert)
   |     |       |      |     |     | +-------------------- Satelliten       -> sats
   |     |       |      |     |     +---------------------- Fix-Qualität     -> has_fix (>0)
   |     |       |      |     +-------------------------- O/W-Indikator (E/W)
   |     |       |      +-------------------------------- Länge  dddmm.mmmm  -> lon
   |     |       +--------------------------------------- N/S-Indikator (N/S)
   |     +----------------------------------------------- Breite ddmm.mmmm   -> lat
   +----------------------------------------------------- UTC-Zeit (ignoriert)
```

Der Talker-Präfix kann variieren (`$GPGGA`, `$GNGGA` bei Multi-GNSS). Der Parser
erkennt den Satz am **`GGA` nach den ersten fünf Zeichen** (`$xxGGA`), nicht am
festen `GP`.

### Kernfehlerquelle: Koordinatenformat `ddmm.mmmm`

NMEA-Koordinaten sind **Grad + Minuten verklebt**, nicht Dezimalgrad:

- Breite: `ddmm.mmmm`  → 2 Stellen Grad + Rest Minuten
- Länge:  `dddmm.mmmm` → 3 Stellen Grad + Rest Minuten

Umrechnung: `dezimal = dd + (mm.mmmm / 60)`, danach Vorzeichen aus dem
N/S- bzw. E/W-Indikator (`S`/`W` → negativ).

Beispiel: `4807.038,N` → 48 Grad + 07.038/60 min = **48.1173°** N.

Das ist die teuerste Falle und wird explizit gegen einen bekannten Wert getestet.

## Checksumme

Jeder NMEA-Satz endet mit `*HH`: das XOR aller Zeichen **zwischen** `$` und `*`
(exklusive), als zweistellige Hex-Zahl. Der Parser berechnet das XOR neu und
vergleicht mit den zwei Hex-Ziffern nach `*`. Bei Mismatch → `false`. Schützt vor
korrupten UART-Zeilen (Rauschen, abgeschnittene Puffer), bevor eine falsche
Position ins SD-Log gerät.

## Tests (TDD, `test/test_gga/`)

RED zuerst — jeder Test muss aus dem *richtigen* Grund fehlschlagen —, dann
minimaler Code (GREEN), dann aufräumen. Getestet wird gegen bekannt-korrekte
Referenzsequenzen, nicht gegen die eigene Annahme.

1. **Bekannter GGA-Satz** → korrekte `lat`/`lon` (ddmm→dezimal geprüft), `alt`,
   `sats`, `has_fix=true`, Rückgabe `true`.
2. **Süd/West** → negative Vorzeichen bei lat/lon.
3. **GGA mit Fix-Qualität `0`** → Rückgabe `true`, aber `has_fix=false`.
4. **Verfälschte Checksumme** → Rückgabe `false`.
5. **Nicht-GGA-Zeile** (`$GPRMC…`) → Rückgabe `false`.
6. **Abgeschnittene / leere Zeile** → Rückgabe `false`, kein Absturz.
7. **Talker-Variante** (`$GNGGA…`) → wird als GGA erkannt.

Double-Asserts sind im native-Env aktiviert (`-D UNITY_INCLUDE_DOUBLE`), da
lat/lon `double` sind. Eigener Test-Ordner mit eigenem `main()`
(`UNITY_BEGIN/END`), wie die anderen Bausteine.

## Nach der Implementierung

`docs/software-flow.md` aktualisieren:
- NMEA-Parser in §6-Tabelle von ⬜ auf ✅.
- `READ_GPS`-Knoten im `loop()`-Diagramm (§3) auf „✅ parse_gga → GpsFix".

## Nicht-Ziele (YAGNI)

- Keine anderen NMEA-Sätze (RMC, GSA, GSV).
- Kein Streaming-/Zeilenpuffer-Handling — der Parser bekommt *eine fertige Zeile*.
  Das Zusammensetzen der UART-Bytes zu Zeilen ist Sache von `src/flight`.
- Keine UTC-Zeit, keine Geschwindigkeit, kein HDOP.
- Kein Kopieren in `TelemetryRecord` im Parser selbst (macht der Aufrufer).
