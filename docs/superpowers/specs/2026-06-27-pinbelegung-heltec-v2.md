# Pinbelegung & Verdrahtung — Heltec WiFi LoRa 32 V2

**Datum:** 2026-06-27
**Status:** Festgelegt (Teil von Schritt 2 — Implementierungsplan)
**Bezug:** `docs/superpowers/specs/2026-06-25-microcontroller-konzept-design.md`

Diese Datei legt **verbindlich** fest, welcher Sensor/Peripherie an welchem GPIO des Heltec V2 hängt.
Sie ist die Referenz für Verdrahtung (Schritt 3) **und** Firmware (Schritt 4). Grundregel:
**board-interne Pins (LoRa, OLED, LED, Button, Vext) NICHT anfassen** — nur freie GPIOs nutzen.

---

## 1. Board-interne Belegung (fix — nicht verwenden!)

Diese Pins sind auf dem Heltec V2 fest mit On-Board-Bauteilen verdrahtet:

| Funktion | GPIO | Hinweis |
|---|---|---|
| **LoRa SCK** (SPI Clock) | **5** | SPI-Bus — wird mit microSD **geteilt** (s. u.) |
| **LoRa MISO** | **19** | SPI-Bus — geteilt mit microSD |
| **LoRa MOSI** | **27** | SPI-Bus — geteilt mit microSD |
| **LoRa NSS/CS** | **18** | nur LoRa |
| **LoRa RST** | **14** | nur LoRa |
| **LoRa DIO0** | **26** | IRQ (TxDone/RxDone) |
| **LoRa DIO1** | **35** | input-only Pin (ok, nur Eingang) |
| **LoRa DIO2** | **34** | input-only Pin (ok, nur Eingang) |
| **OLED SDA** (I²C) | **4** | I²C-Bus — wird mit BMP280/MPU-6050 **geteilt** |
| **OLED SCL** (I²C) | **15** | I²C-Bus — geteilt |
| **OLED RST** | **16** | ⚠️ **NICHT** für GPS verwenden (Tutorial-Falle, s. §4) |
| **Onboard-LED** | **25** | frei umnutzbar, bleibt aber als Status-LED |
| **PRG / User-Button** | **0** | Boot-Strapping-Pin |
| **Vext-Steuerung** | **21** | schaltet 3,3-V-Rail für OLED/Sensoren (LOW = AN) |

---

## 2. Unsere Peripherie — Verdrahtungstabelle (verbindlich)

| Komponente | Bus | Signal | → Heltec-GPIO | Spannung | Bemerkung |
|---|---|---|---|---|---|
| **NEO-6M GPS** | UART2 | GPS **TX** → ESP32 **RX** | **23** | 3,3 V | ESP32 empfängt NMEA |
| | | GPS **RX** ← ESP32 **TX** | **17** | 3,3 V | **nötig** für UBX-Flight-Mode-Befehl! |
| | | VCC / GND | 3V3 / GND | | 9600 Baud |
| **BMP280** | I²C | SDA / SCL | **4 / 15** | 3,3 V | teilt I²C-Bus mit OLED; Adr. 0x76/0x77 |
| **MPU-6050** | I²C | SDA / SCL | **4 / 15** | 3,3 V | teilt I²C-Bus; Adr. 0x68/0x69 |
| **DS18B20** (außen) | 1-Wire | DATA | **22** | 3,3 V | **4,7 kΩ Pull-up** DATA→3V3 nötig |
| **GUVA-S12SD** (UV) | Analog | OUT (Aout) | **36** (VP) | 3,3 V | **ADC1**, input-only — perfekt für `analogRead()` |
| **microSD-Modul** | SPI | SCK / MISO / MOSI | **5 / 19 / 27** | s. §4 | **gemeinsamer SPI-Bus mit LoRa** |
| | | **CS** (eigener!) | **13** | | eigener Chip-Select, getrennt von LoRa-CS (18) |
| **Strom** | — | 4× Li-AA (Reihe) | **5V/VIN-Pin** | ~6 V | **nicht** an LiPo-JST! Stützelko an 3V3 |

---

## 3. Freie GPIO-Reserve (nach dieser Belegung noch verfügbar)

| GPIO | Eignung | Idee für später |
|---|---|---|
| **32** | ADC1, Touch, I/O | z. B. Action-Cam-Trigger / 2. Analogsensor |
| **33** | ADC1, Touch, I/O | frei |
| **39** (Sensvn) | ADC1, **input-only** | weiterer Analogeingang |
| **37** (CapVP) | ADC1, **input-only** | weiterer Analogeingang (Cap-Sense) |
| **38** (CapVN) | ADC1, **input-only** | weiterer Analogeingang (Cap-Sense) |
| **2** | I/O, ⚠️ Strapping | nur mit Vorsicht (beim Boot nicht hochziehen) |
| **12** | I/O, ⚠️ Strapping (MTDI) | nur mit Vorsicht (beim Boot LOW halten) |
| **25** | LED | bleibt Status-LED |

> **Korrektur (lt. Pinout-Diagramm):** GPIO **37/38** sind beim Heltec V2 **doch herausgeführt**
> (`CapVP`/`CapVN`, beide ADC1, input-only) → als weitere Analogeingänge nutzbar.

---

## 4. Kritische Hinweise (bitte beim Aufbau beachten)

1. **GPS-Pins ≠ Tutorial-Pins.** Viele ESP32-Tutorials (z. B. RandomNerd) nutzen GPIO16/17 für UART2.
   **GPIO16 ist beim Heltec V2 der OLED-Reset!** Darum hier bewusst **GPIO23 (RX) / GPIO17 (TX)**.
2. **ESP32-TX zum GPS ist Pflicht.** Der Flight-Mode (UBX-CFG-NAV5, „Airborne <2g") wird per Befehl
   **an** das GPS gesendet → die TX-Leitung (GPIO17 → GPS-RX) muss verdrahtet sein, nicht nur RX.
3. **UV nur an ADC1 (GPIO36).** ADC2 wird bei aktivem Funk blockiert. GPIO36 ist input-only — für einen
   reinen Analogeingang ideal. Mehrfach lesen + mitteln (ADC rauscht). Rohwerte loggen, nicht „UV-Index".
4. **SPI-Bus geteilt (LoRa + SD).** SCK/MISO/MOSI (5/19/27) werden gemeinsam genutzt; nur die **CS-Pins**
   unterscheiden (LoRa = 18, SD = 13). Funktioniert, wenn immer nur **ein** CS aktiv ist (Bibliotheken
   regeln das). Falls im Test instabil → microSD auf eigenen HSPI-Bus legen (Reserve-Pins 12/13/+).
5. **microSD-Spannung testen.** Viele SD-Module brauchen **5 V** (Onboard-Regler + Levelshifter), manche
   laufen an 3,3 V. **Vor dem Aufbau am Heltec prüfen** (Konzept §10). Das Datensignal bleibt 3,3 V.
6. **Vext einschalten.** GPIO21 zu Beginn von `setup()` auf **LOW** → versorgt OLED (und ggf. Sensoren am
   Vext-Rail) mit 3,3 V. Bleibt im Flug dauerhaft AN.
7. **1-Wire Pull-up.** DS18B20-DATA (GPIO22) braucht **4,7 kΩ** nach 3V3, sonst kein gültiges Signal.
8. **Strapping-Pins 0/2/12/15** nicht mit Lasten beschalten, die den Boot stören (15 ist ohnehin OLED-SCL).

---

## 5. Direkt verwendbar: `pins.h` (für Schritt 4)

```cpp
// pins.h — Heltec WiFi LoRa 32 V2 — Wetterballon
// Board-intern (Referenz, von Heltec-Lib gesetzt): LoRa SCK5 MISO19 MOSI27 NSS18 RST14
//   DIO0 26 DIO1 35 DIO2 34 | OLED SDA4 SCL15 RST16 | LED25 | BTN0 | Vext21

// --- Vext (3,3-V-Rail für OLED/Sensoren) ---
#define PIN_VEXT        21   // LOW = AN

// --- GPS (NEO-6M) an UART2 / Serial2, 9600 Baud ---
#define PIN_GPS_RX      23   // ESP32-RX  <- GPS-TX
#define PIN_GPS_TX      17   // ESP32-TX  -> GPS-RX (Pflicht: Flight-Mode-Befehl!)
#define GPS_BAUD        9600

// --- I2C (geteilt mit OLED) : BMP280 + MPU-6050 ---
#define PIN_I2C_SDA      4
#define PIN_I2C_SCL     15

// --- DS18B20 (1-Wire, 4,7k Pull-up nach 3V3) ---
#define PIN_ONEWIRE     22

// --- GUVA-S12SD UV (ADC1, input-only) ---
#define PIN_UV_ADC      36   // analogRead(); mehrfach mitteln

// --- microSD (SPI, Bus geteilt mit LoRa: SCK5/MISO19/MOSI27) ---
#define PIN_SD_CS       13   // eigener Chip-Select (LoRa-CS = 18)

// --- Onboard-Status-LED ---
#define PIN_LED         25
```
