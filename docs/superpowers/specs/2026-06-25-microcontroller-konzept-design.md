# Technisches Konzept: Microcontroller-System für Schul-Wetterballon

**Datum:** 2026-06-25
**Status:** Konzept (Design abgenommen)
**Kontext:** Schulprojekt Wetterballon. Ballon, Hülle und Mechanik stehen bereits.
Dieses Dokument beschreibt ausschließlich das Elektronik-/Microcontroller-System.

---

## 1. Ziele & Randbedingungen

| Punkt | Festlegung |
|---|---|
| **Hauptzweck** | Kombination aus (1) lokalem Daten-Logging und (2) Live-Tracking zur Bergung |
| **Flugzeit** | ca. 2–3 Stunden (Aufstieg + Sinkflug) |
| **Gewichtslimit** | **< 200 g** (nur Elektronik-Box; Fallschirm/Schnur/Ballon zählen separat) |
| **Budget** | sparsam, **< 100 €** für die Basis-Elektronik |
| **Box/Dämmung** | bereits vorhanden — nicht im Budget/Gewichtskauf eingerechnet (Dämmung trotzdem genutzt) |
| **Programmierung** | C++/Arduino (Projektbetreuer ist Softwareentwickler, daher frei wählbar) |
| **Bodenstation** | muss mit erstellt werden (LoRa-Empfänger + Laptop) |

### Messgrößen (Basis)
- Temperatur, Luftdruck (Atmosphäre)
- Höhe & Position (GPS) — zugleich Grundlage fürs Wiederfinden
- Beschleunigung, Drehung, Lage (IMU)
- Außentemperatur getrennt von der Box-Innentemperatur
- **UV-Strahlung** (als spannendes Höhen-Experiment aufgenommen)

---

## 2. Systemarchitektur

```
                    ┌─────────────────────────────────────┐
                    │   FLUG-EINHEIT (Nutzlast, <200 g)     │
                    │                                       │
   [GPS-Antenne]────┤  ┌─────────────────────────────┐     │
                    │  │  Heltec WiFi LoRa 32 V2      │     │
                    │  │  • ESP32 (Steuerung)         │     │
                    │  │  • LoRa SX1276 868 (intern) ─┼─────┼──► Funk zur Bodenstation
                    │  │  • OLED-Display (intern)     │     │
   [LoRa-Antenne]───┤  │  UART├── NEO-6M GPS (Flight-Mode)   │
                    │  └──────┬───────────────┬───────┘     │
                    │     I²C │           SPI │             │
                    │  ┌──────┴──────┐  ┌─────┴──────┐      │
                    │  │ BMP280      │  │ microSD     │      │
                    │  │ (T/Druck)   │  │ (Logger)    │      │
                    │  │ MPU-6050    │  └─────────────┘      │
                    │  │ (IMU/Lage)  │   1-Wire              │
                    │  │ GUVA (UV,A) │  ┌─────────────┐      │
                    │  └─────────────┘  │ DS18B20      │     │
                    │   (GUVA: analog)  │ (Außentemp.) │     │
                    │                   └─────────────┘      │
                    │  Stromversorgung: 4× Energizer Lith.AA  │
                    │  in vorhandener Styropor-Box (Dämmung)  │
                    └─────────────────────────────────────┘

   ┌──────────────────────────────────────────┐
   │   BODENSTATION                            │
   │   2. LoRa-Modul (RX) + Laptop/Handy       │
   │   → Live-Position auf Karte + Logfile     │
   └──────────────────────────────────────────┘
```

**Kernidee:** Als Kern dient das **Heltec WiFi LoRa 32 V2** — ein Board, das **ESP32 (Steuerung),
LoRa-Funk (SX1276, 868 MHz) und ein OLED-Display** vereint, inkl. Antenne und LiPo-Lademanagement.
Es ist der praktische Mittelweg zwischen T-Beam (teuer/knapp) und reinem Einzelteil-Aufbau: LoRa ist
bereits intern mit dem ESP32 verdrahtet (**keine LoRa-Lötarbeit**), und alles ist über einen Shop
(Funduino) verfügbar. Das GPS (NEO-6M) wird separat per UART angebunden. Periphere Komponenten
hängen über einfache Busse dran:

- **UART**: NEO-6M GPS (4 Drähte: VCC, GND, TX, RX; 9600 Baud)
- **SPI**: microSD-Logger (eigener CS-Pin; LoRa-SPI ist board-intern)
- **I²C** (gemeinsame 2-Draht-Leitung): BMP280, MPU-6050 (teilen sich den Bus mit dem OLED)
- **Analog (ADC)**: GUVA-S12SD (UV-Sensor) — an einem freien ADC1-Pin des ESP32
- **1-Wire**: DS18B20 (Außentemperatur, an kurzem Kabel außerhalb der Box)

Der Code führt beim Start einen **I²C-Scan** durch und erkennt automatisch, welche Sensoren
bestückt sind — das System läuft auch, wenn eine Komponente fehlt.

---

## 3. Komponentenliste (Basis)

| Komponente | Modell | Bus | Misst / Zweck | Gewicht | Preis (ca.) |
|---|---|---|---|---|---|
| **Kern: ESP32+LoRa** ✅ **gekauft** | Heltec WiFi LoRa 32 V2 (ESP32 + SX1276 868 MHz + OLED + Antenne) | SPI (intern) | Steuerung + LoRa-Telemetrie + Display | ~15 g | 33 € |
| **Kern: GPS** ✅ **gekauft** | u-blox NEO-6M (mit Antenne) | UART | Position, Höhe, Flight-Mode (50 km) | ~15 g | 12 € |
| **Atmosphäre** ✅ **gekauft** | BMP280 | I²C | Temperatur, Luftdruck (in Box; keine Feuchte) | ~1 g | 3 € |
| **Lage/Bewegung** ✅ **gekauft** | MPU-6050 (6-Achsen; Alternative: LSM6DSOX) | I²C | Beschleunigung, Drehrate, Lage (kein Kompass) | ~1 g | 4 € |
| **Außentemperatur** ✅ **gekauft** | DS18B20 (wasserdicht, 1 m Kabel) | 1-Wire | Außentemp. bis ca. −55 °C, getrennt von Box | ~5 g | 3 € |
| **UV** ✅ **gekauft** | GUVA-S12SD | Analog (ADC) | UV-Strahlung inkl. UV-B (Höhen-Effekt) | ~1 g | 7 € |
| **Logger** ✅ **gekauft** | SPI MicroSD-Modul (TF-Card Reader) + Karte | SPI | lokales, hochauflösendes Log | ~3 g | 2 € + Karte |
| **Strom** ✅ **gekauft** | 4× Energizer Ultimate Lithium AA + Halter | — | Versorgung bis −40 °C | ~60 g | 12 € |
| **Kamera** (Erweiterung) | leichte Action-Cam (~50–70 g), eigener Akku + SD | — | lokale Bild-/Videoaufzeichnung; ⚠️ Gewicht/Kälte/Laufzeit beachten | ~60 g | ~40 € |
| **Antennen** | LoRa-868 (beim Heltec dabei) + GPS-Antenne (beim NEO-6M dabei) | — | LoRa-Reichweite + GPS-Empfang | — | — |
| **Verkabelung/Kleinteile** ✅ **gekauft** | Dupont-Kabel, Litze, Stiftleisten, Kondensatoren (Elko 100–470 µF + 100 nF) | — | Aufbau (LoRa schon intern → weniger Löten) | ~5 g | 10 € |
| | | | **Summe Basis** | **~117 g** | **~84 €** (+ microSD-Karte) |

> **Box/Dämmung** sind vorhanden und daher nicht eingerechnet. Bei Bedarf Dämmung der Box mit
> einplanen — selbst dann bleibt deutlich Reserve unter 200 g.

**Bewusste Doppelmessung der Höhe:** Höhe wird sowohl per **GPS** (geometrisch) als auch aus dem
**Luftdruck** (BMP280, barometrisch) bestimmt. Schöner Vergleich für den Unterricht.

### UV-Sensor: Wahl, Vorteile & Fallstricke (GUVA-S12SD)

**Entscheidung:** **GUVA-S12SD** (analog) statt LTR390 (I²C).

**Warum der GUVA:**
- Misst auch **UV-B** (Spektrum ~240–370 nm), nicht nur UV-A. Gerade UV-B nimmt mit der Höhe
  besonders stark zu (über der Ozonschicht) → **physikalisch der interessantere Effekt** für unser Experiment.
- Sehr **einfacher Code**: nur `analogRead()` an einem Pin — keine Bibliothek, kein Bus, keine Adresse.
- Günstig und in DE gut verfügbar (Funduino ~6,21 €, BerryBase/Adafruit ~7,15 €).

**Fallstricke — bewusst einplanen:**

| Thema | Problem | Maßnahme |
|---|---|---|
| **ESP32-ADC zickig** | Der Analog-Wandler des ESP32 ist nicht ganz linear und rauscht. | Mehrfach lesen + Mittelwert bilden; ggf. grob kalibrieren. |
| **Pin-Wahl** | Nur **ADC1**-Pins (GPIO 32–39) funktionieren, wenn LoRa/WLAN aktiv ist (ADC2 wird vom Funk blockiert). | Freien ADC1-Pin am ESP32 wählen und in der Pinbelegung fest dokumentieren. |
| **Rohwerte statt „UV-Index"** | Eine fertige UV-Index-Umrechnung gilt nur am Boden, nicht in 30 km. | **Rohspannung/ADC-Counts loggen**, Auswertung relativ (Anstieg mit Höhe) statt absoluter Index. |
| **Sättigung** | In großer Höhe ist die UV-Strahlung extrem stark → Sensor/ADC könnten am oberen Anschlag „abschneiden“. | Messbereich vor dem Flug prüfen; Spannungsteiler/Bereich so wählen, dass der Maximalwert nicht erreicht wird. |
| **UV-Fenster** | Normales Glas & Acryl (Plexiglas) **blockieren UV** → Sensor würde nichts sehen. | Sensor schaut durch ein **kleines Loch nach oben** zum Himmel; bei Bedarf dünne **PTFE-Folie** als Wetterschutz (geringer UV-Verlust). **Quarzglas** wäre ideal, aber teuer/unnötig. Vor dem Flug gegen Sonne testen. |
| **Kälte** | −40 °C und kälter in der Höhe. | Sensor in/an der gedämmten Box; im Kältetest (Gefrierfach) verifizieren. |

> **Alternative LTR390 (I²C):** wäre softwareseitig robuster (kein ADC-Stress, kein Extra-Pin, hängt am
> vorhandenen I²C-Bus), misst aber v. a. UV-A. Bewusst zugunsten des UV-B-fähigen GUVA zurückgestellt.

### Kamera (Erweiterung): Action-Cam mit lokaler Aufzeichnung

**Entscheidung:** Eine **leichte Action-Cam** fliegt als **eigenständiges System** mit und zeichnet
**lokal auf eigene Speicherkarte** auf (Bilder/Video). **Keine** Live-Übertragung per LoRa — dafür ist
LoRa viel zu langsam (ein Foto bräuchte Minuten) und der EU-Duty-Cycle (~1 % Sendezeit) lässt es nicht zu.
Bilder werden **nach der Bergung** von der Karte ausgewertet. Die Cam ist vom Rest der Elektronik
**unabhängig** (eigener Akku, eigene SD) → fällt eines aus, läuft das andere weiter.

**Warum Action-Cam statt ESP32-CAM:** beste Bildqualität (Full-HD-Video statt 2-MP-Fotos), keine
zusätzliche Programmierung. Preis: eigenes Gerät + Gewicht.

**Kritische Fallstricke — bewusst einplanen:**

| Thema | Problem | Maßnahme |
|---|---|---|
| **Gewicht** | Flug-Einheit ~117 g + Cam 60–90 g → nahe am 200-g-Limit. Eine GoPro (~125 g) sprengt es. | **Leichtes Modell (~50–70 g)** wählen; Gesamtgewicht vor dem Flug wiegen. |
| **Akku-Kälte** | Action-Cam-LiPo bricht bei −40 °C ein / schaltet ab. | Cam **innen in die gedämmte Box** mit Sichtfenster; im Kältetest prüfen. |
| **Laufzeit** | Viele Cams laufen nur 1–2 h, Flug dauert 2–3 h → Cam ginge evtl. vor der Landung aus. | Laufzeit vorab testen; ggf. Intervall-/Foto-Modus statt Dauervideo; größeren Akku/Powerbank prüfen. |
| **Sichtfenster** | Linse braucht freie Sicht (nach unten/schräg). Beschlag/Vereisung möglich. | Klares Fenster (Cam sieht durch Kunststoff — anders als UV!); Anti-Beschlag beachten; Linse nicht von Box/Antenne verdecken. |
| **Beschlag/Kondens** | Temperatursturz → Linse beschlägt von innen. | Cam vor dem Start „warm und trocken“ einpacken; ggf. Silikagel in die Box. |

> **Live-Bilder (SSDV) als späteres Stretch-Goal:** Mini-Bilder ließen sich theoretisch per LoRa
> herunterfunken (SSDV-Verfahren), liefern aber nur wenige, stark gepixelte Bilder und teilen sich die
> Funkzeit mit der Telemetrie. Für die erste Flugversion **nicht** vorgesehen.

---

## 4. Verworfene Optionen (dokumentiert)

Diese Erweiterungen wurden geprüft und **bewusst nicht** umgesetzt. Festgehalten, damit die
Begründung für eine spätere Erweiterung nachvollziehbar ist.

| Option | Beispiel-Modul | Grund für Verzicht |
|---|---|---|
| **GSM-Bergungsmodul** | SIM800L + eigene Zelle | **Gegen GSM entschieden.** Würde ~50 g zusätzlich kosten (Modul + eigene Batterie + Stützkondensator) und damit die 200 g sprengen. Stromspitzen bis ~2 A bräuchten einen komplett eigenen Stromkreis. LoRa allein findet den Ballon zuverlässig. Konzept steht bereit, falls später gewünscht (eigene Zelle, eigener Stromkreis, UART-Anbindung, nur am Boden aktiv). |
| **Geigerzähler** | Pocket Geiger / RadiationD | **Zu teuer** (~50–100 €) und zu schwer fürs Budget/Gewicht. |
| **Luftqualität** | SCD40 (CO₂) / SGP40 | Funktion in Vakuum/extremer Kälte fraglich; vorrangig bodennah sinnvoll. |

---

## 5. Stromversorgung & Laufzeit

**Auslegung**
- **4× Energizer Ultimate Lithium AA** in Reihe → ~6 V nominal, ~3000 mAh, ~60 g, zuverlässig bis −40 °C.
- Versorgung über den 5V/VIN-Eingang des Heltec-Boards (dessen Onboard-Regler liefert 3,3 V für
  ESP32, das interne LoRa, GPS und Sensoren). ⚠️ Strombedarf des LoRa beim Senden (Spitzen ~120 mA)
  mit Stützkondensator abfangen. Das integrierte LiPo-Management des Heltec wird hier **nicht**
  genutzt (wir versorgen über VIN aus den Lithium-AA).

> ⚠️ **Spannungs-Check offen (laut offiziellem V2-Datenblatt, Tab. 3-2):** Der **5V-Pin akzeptiert
> nur 4,7–6,0 V**. Energizer L91 haben **frisch ~1,8 V Leerlauf** → 4 Zellen können **>6 V** liefern
> und das Maximum überschreiten. **Vor dem Flug zu messen** (siehe TODO „Batteriespannung messen"):
> reale Spannung frisch + unter Last + kalt. Falls >6,0 V → **Step-Down auf 5,0 V** (z. B. MP1584)
> zwischen Batterie und 5V-Pin einplanen. Strombedarf selbst ist unkritisch (Datenblatt bestätigt
> LoRa 20 dB = 130 mA; 3V3-Pin liefert bis 500 mA, Vext bis 350 mA).

**Verbrauchsschätzung**

| Zustand | Stromaufnahme |
|---|---|
| GPS aktiv + Sensoren lesen + SD schreiben | ~120 mA (Dauer) |
| LoRa-Sendespitze (kurz, gepulst) | +90 mA kurzzeitig |
| **realistischer Mittelwert** | **~130–150 mA** |

**Laufzeit:** 3000 mAh ÷ ~150 mA ≈ **~20 h theoretisch**. Mit Kälteverlusten und Sicherheitsabschlag
bleiben **deutlich > 10 h** → Flug (2–3 h) plus große Bergungsreserve.

**Designregel Bergung:** Nach erkannter Landung wechselt die Software in einen **Spar-Bergungsmodus**
(seltener loggen, GPS-Position regelmäßig per LoRa als Bake senden) → Position-Bake hält viele Stunden.

---

## 6. Software-Konzept (ESP32 / Arduino-C++)

### Flugphasen (automatisch über Höhe/Steig-/Sinkrate erkannt)

| Phase | Erkennung | Verhalten |
|---|---|---|
| **PreFlight** | GPS-Fix, Höhe niedrig & stabil | schnelles Logging, häufiges LoRa-Senden (Funktest) |
| **Ascent** | Höhe steigt | volles Logging (~1 Hz auf SD), LoRa-Telemetrie alle paar Sek. |
| **Burst/Descent** | Höhe sinkt schnell | wie Ascent, ggf. höhere Lograte um den Burst |
| **Landed** | Höhe niedrig & stabil | Bergungsmodus: selten loggen, GPS-Bake per LoRa |

### Datenfluss pro Messzyklus
```
  Sensoren lesen (GPS, BMP280, MPU-6050, DS18B20, GUVA-S12SD)
        │
        ▼
  Datensatz bauen (Zeitstempel + alle Werte als CSV-Zeile)
        │
        ├──► auf microSD schreiben   (lückenloses, hochauflösendes Log)
        │
        └──► kompakte Telemetrie per LoRa senden (Position + Kernwerte)
                    │
                    ▼
            Bodenstation: empfängt, zeigt Position auf Karte, loggt mit
```

### Wichtige Software-Bausteine
1. **GPS-Flight-Mode-Init (kritisch):** Unser **NEO-6M** beim Start einmalig per UBX-Befehl
   (UBX-CFG-NAV5) in den **„Airborne <2g / Navigation Mode 6"** schalten. Ohne diesen Schritt
   schalten viele GPS-Module oberhalb ~18 km ab (CoCom-Begrenzung). Dies ist der häufigste Fehler bei
   Höhenballons und muss zwingend implementiert und am Boden getestet werden. Der NEO-6M
   unterstützt Mode 6 (max. ~50 km), ist der **klassische, bestens dokumentierte HAB-Chip** und
   über UART (9600 Baud) angebunden.
2. **Sensor-Auto-Detect:** beim Boot I²C-Scan → System läuft auch bei fehlendem Sensor.
3. **Robustheit:** Watchdog-Timer (Neustart bei Hänger); jeder Datensatz sofort auf SD
   geschrieben (Flush) → kein Datenverlust bei Stromausfall/Reset.
4. **CSV-Format:** direkt in Excel/Python auswertbar (Unterricht).
5. **LoRa-Telemetrie:** kompaktes Textformat; optional später an HAB-Tracking-Netze anbindbar.

---

## 7. Bodenstation (detailliert)

Die Bodenstation empfängt die LoRa-Telemetrie des Ballons, zeigt die Position live auf einer
Karte und protokolliert alles mit. Sie ist bewusst einfach gehalten: ein einzelnes
ESP32-LoRa-Board am Laptop genügt.

### 7.1 Hardware

| Komponente | Modell | Zweck | Preis (ca.) |
|---|---|---|---|
| Empfänger-Board | **Heltec WiFi LoRa 32 V3** (ESP32-S3, SX1262, 868 MHz, OLED) | LoRa-Empfang + Statusanzeige | ~20 € |
| Antenne | 868-MHz-Antenne (SMA), besser als Stummelantenne | Reichweite | ~5 € |
| USB-Kabel | USB-C Datenkabel | Verbindung zum Laptop | vorhanden |
| (optional) Yagi-Richtantenne | 868 MHz Yagi | deutlich mehr Reichweite, wenn der Ballon weit weg ist | ~25 € |

> **Wichtig:** Sende- und Empfangs-Board müssen die **gleiche Frequenz (868 MHz)** und die
> **gleichen LoRa-Parameter** (Spreading Factor, Bandbreite, Coding Rate, Sync Word) nutzen,
> sonst „sehen" sie sich nicht. Diese Parameter werden im Code beider Seiten identisch gesetzt.
>
> Das Empfänger-Board (Heltec V3, SX1262) und unser Flug-Board (Heltec V2, SX1276) sind zwar
> unterschiedliche Funkchips, aber **LoRa-kompatibel**, solange die Parameter übereinstimmen.
> Am einfachsten: als Empfänger ebenfalls ein Heltec-Board verwenden — dann ist die Software
> auf beiden Seiten nahezu identisch.

### 7.2 Software-Kette

```
   [Ballon] ──LoRa──► [Heltec V3 Empfänger] ──USB-Seriell──► [Laptop]
                                                                │
                          ┌─────────────────────────────────────┤
                          ▼                                     ▼
                  Live-Karte (Browser)                   Logfile (CSV)
```

1. **Empfänger-Firmware (ESP32, Arduino):** empfängt die LoRa-Pakete und gibt jede Telemetrie-Zeile
   unverändert über USB-Seriell aus (gleiches CSV-/Textformat wie der Ballon sendet).
2. **Anzeige am Laptop** — zwei einfache Varianten, je nach gewünschtem Aufwand:
   - **Variante A (minimal):** Serieller Monitor / einfaches Python-Skript liest die serielle
     Schnittstelle, schreibt in eine CSV und gibt Position als Text aus. Reicht zum Wiederfinden
     (Koordinaten ins Handy/Google Maps eintippen).
   - **Variante B (komfortabel, empfohlen):** kleines Python-Skript (z. B. mit `pyserial`),
     das die Position live auf einer Karte zeigt — entweder lokal mit einer Karten-Bibliothek
     oder durch Erzeugen einer **GPX-Datei**, die man in eine Karten-App lädt. Schreibt
     gleichzeitig das Logfile.
3. **Format:** Da Ballon-Log und Empfangs-Log dasselbe CSV-Format haben, lassen sich nach dem
   Flug beide Datensätze leicht vergleichen (Bord-SD vs. empfangene Telemetrie).

### 7.3 Praktische Hinweise zur Bergung

- Die Bodenstation muss **nicht** den ganzen Flug empfangen — entscheidend ist die **letzte
  empfangene Position** vor/bei der Landung. Diese immer sofort notieren.
- Höhe hilft der Reichweite: Empfänger an einen erhöhten Standort / freie Sicht bringen.
- Der Ballon sendet nach der Landung im **Bergungsmodus** weiter (siehe Abschnitt 5) → mit einer
  Richtantenne kann man sich der letzten Position annähern und das Signal „anpeilen".

---

## 8. Rechtlicher Hinweis (organisatorisch, außerhalb der Elektronik)

- Ballonaufstieg braucht in Deutschland eine **Aufstiegserlaubnis** der zuständigen
  Landes-Luftfahrtbehörde (regional, z. B. Bezirksregierung / Regierungspräsidium).
- Unter **4 kg** Gesamtmasse gilt der Ballon als **„leicht"** → vereinfachtes Verfahren.
  Eure Nutzlast (< 200 g Elektronik + Ballon/Schirm) liegt weit darunter.
- Für **Schulen/Forschung** ist das Verfahren in der Regel **gebührenfrei**.
- LoRa auf **868 MHz** ist in DE lizenzfrei (SRD-Band, Duty-Cycle-Grenzen beachten).
- Haftpflichtversicherung für den Aufstieg klären (oft über die Schule abgedeckt).

---

## 9. Gewichts- & Budget-Zusammenfassung

| | Gewicht | Budget |
|---|---|---|
| Basis-Elektronik (Flug-Einheit) | **~117 g** | **~84 €** (+ microSD-Karte) |
| Reserve bis Limit | ~83 g | ~16 € |
| Bodenstation (separat) | — | ~25 € |

→ Beide Vorgaben (**< 200 g**, **< 100 €**) für die Flug-Einheit eingehalten. Der Eigenbau-Kern
  (Heltec WiFi LoRa 32 V2 ~33 € + NEO-6M ~12 € = ~45 €) ist günstiger als das fertige T-Beam-Board
  (~50 €), besser verfügbar **und** spart die LoRa-Lötarbeit (LoRa ist im Heltec schon integriert).
  Bodenstation kommt separat hinzu (~25 €).

---

## 10. Bezugsquellen

**Empfehlung:** Der Kern besteht aus dem **Heltec WiFi LoRa 32 V2** (ESP32 + LoRa + OLED) plus einem
separaten **NEO-6M-GPS**. Das Heltec-Board und die meisten Sensoren gibt es bei **Funduino** (DE-Versand);
weitere Teile/die Bodenstation bei **BerryBase**. Alle Teile sind bei gängigen DE-Händlern gut
verfügbar — kein Spezial-Shop nötig. Eine zentrale Bestellung bei wenigen Händlern hält Versandkosten niedrig.

### Flug-Einheit

| Komponente | Bevorzugte Quelle | Hinweis |
|---|---|---|
| **Heltec WiFi LoRa 32 V2** ✅ **gekauft** | Funduino (~32,90 €) | ESP32 + LoRa SX1276 (863–928 MHz) + OLED + Antenne; LoRa intern verdrahtet (keine Lötarbeit). |
| **u-blox NEO-6M (mit Antenne)** ✅ **gekauft** | Funduino / BerryBase (~12 €) | GPS; UART. Flight-Mode 6 (max. 50 km), klassischer HAB-Chip. |
| BMP280 ✅ **gekauft** | BerryBase | T/Druck (keine Feuchte) |
| MPU-6050 (6-Achsen) ✅ **gekauft** | Funduino (~3,62 €) | IMU/Lage; Alternative LSM6DSOX (BerryBase/Adafruit) |
| GUVA-S12SD (UV) | Funduino (~6,21 €) / BerryBase (Adafruit ~7,15 €) | analoger UV-Sensor inkl. UV-B; Alternative LTR390 (I²C) |
| DS18B20 (wasserdicht, 1 m Kabel) ✅ **gekauft** | BerryBase | Außentemperatur |
| SPI MicroSD-Modul ✅ **gekauft** | Funduino (~1,72 €) | SPI-Logger (eigener CS-Pin). ⚠️ am Heltec testen: läuft es an 3,3 V oder braucht es 5 V? |
| microSD-Karte (klein, FAT32, ≤ 32 GB) ✅ **gekauft** | Funduino / Elektronikhandel | separat zum Modul; kleine Karte reicht, FAT32 formatieren |
| 4× Energizer Ultimate Lithium AA + Halter ✅ **gekauft** | Elektronik-/Supermarkt | Lithium, **nicht** Alkaline. Halter: 4×AA in **Reihe** (6 V). An **5V-Pin** des Heltec, **nicht** LiPo-JST! |
| Kabel, Stützkondensatoren, Header | BerryBase | Kleinteile |

### Bodenstation

| Komponente | Bevorzugte Quelle | Hinweis |
|---|---|---|
| Heltec WiFi LoRa 32 (V2 oder V3, 868 MHz) | Funduino / BerryBase | Empfänger; baugleich zum Flug-Board → identische Software |
| 868-MHz-Antenne (SMA) | BerryBase | bessere Reichweite |
| (optional) 868-MHz-Yagi | BerryBase / Funk-Shop | Anpeilen bei der Bergung |

> **Preise/Verfügbarkeit ändern sich** — vor der Bestellung auf den Produktseiten gegenprüfen.
> Bestellzeit für die Lieferung und einen Puffer für Tests einplanen (GPS-Flight-Mode-Test!).

---

## 11. Referenzen

- GPS-Höhe / CoCom / Flight-Mode: UKHAS Wiki — https://ukhas.org.uk/doku.php?id=guides:gps_modules
- GPS in großer Höhe: BigRedBee — https://shop.bigredbee.com/blogs/news/high-altitude-gps-operation
- Heltec WiFi LoRa 32 V2 Doku/Pinout: Heltec — https://docs.heltec.org/en/node/esp32/wifi_lora_32/index.html
- Heltec V2 Pinout (PDF) — https://resource.heltec.cn/download/WiFi_LoRa_32/WIFI_LoRa_32_V2.pdf
- ESP32 + NEO-6M GPS (Verdrahtung/Code): Random Nerd Tutorials — https://randomnerdtutorials.com/esp32-neo-6m-gps-module-arduino/
- LoRa-HAB-Empfänger (Beispiel): TBTracker-RX — https://github.com/RoelKroes/TBTracker-RX
- Aufstiegserlaubnis DE: Stratoflights — https://www.stratoflights.com/tutorial/wetterballon-anmeldung-versicherung/deutschland/
- Aufstiegserlaubnis (Behörde): Reg. Oberbayern — https://www.regierung.oberbayern.bayern.de/aufgaben/37200/37222/leistung/leistung_50751/index.html
- BerryBase BMP280 — https://www.berrybase.de/bmp280-barometrischer-sensor-fuer-temperatur-und-luftdruck
- Funduino MPU-6050 — https://funduinoshop.com/elektronische-module/sensoren/bewegungs-und-distanzsensoren
- Funduino GUVA-S12SD (UV) — https://funduinoshop.com/en/electronic-modules/sensors/light-color/guva-s12sd-uv-sensor
- BerryBase GUVA-S12SD (UV) — https://www.berrybase.de/en/analogue-uv-light-sensor-breakout-guva-s12sd
- LTR390 (UV, Alternative) — https://www.berrybase.de/adafruit-ltr390-uv-licht-sensor-stemma-qt-qwiic
- BerryBase DS18B20 — https://www.berrybase.de/en/ds18b20-ic-digital-temperature-sensor
- Funduino SPI MicroSD-Modul — https://funduinoshop.com/en/electronic-modules/other/memory-logger/spi-microsd-card-module-tf-card-reader
- Funduino Heltec WiFi LoRa 32 V2 — https://funduinoshop.com/elektronische-module/wireless-iot/esp-wifi/heltec-lora-32-v2-863-928-mhz/esp32/oled
- NEO-6M GPS Flight-Mode (Mode 6, 50 km, UBX-CFG-NAV5) — u-blox-Doku / AVA HAB (https://ava.upuaut.net/?p=750)
