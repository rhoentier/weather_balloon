// pins.h — Heltec WiFi LoRa 32 V2 — Wetterballon Flug-Einheit
// Quelle: docs/superpowers/specs/2026-06-27-pinbelegung-heltec-v2.md
// Board-intern (von Heltec-Lib gesetzt): LoRa SCK5 MISO19 MOSI27 NSS18 RST14
//   DIO0 26 DIO1 35 DIO2 34 | OLED SDA4 SCL15 RST16 | LED25 | BTN0 | Vext21

#ifndef FLIGHT_PINS_H
#define FLIGHT_PINS_H

// --- Vext (3,3-V-Rail für OLED/Sensoren) ---
#define PIN_VEXT        21   // LOW = AN

// --- GPS (NEO-6M) an UART2 / Serial2, 9600 Baud ---
// WICHTIG: UART wird gekreuzt verkabelt (TX des einen an RX des anderen):
//     ESP32 GPIO17 (TX) --> GPS RX   (Befehl raus, z.B. Flight-Mode)
//     ESP32 GPIO23 (RX) <-- GPS TX   (NMEA + UBX-ACK rein)
// Am Board verifiziert: mit dieser Kreuzung kommt der UBX-ACK zurück.
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
// WICHTIG: Das sind NICHT die ESP32-VSPI-Standardpins (SCK18/MISO19/MOSI23)!
// Ohne explizites SPI.begin(SCK,MISO,MOSI,CS) nutzt die SD-Lib die falschen
// Default-Pins und die Karte antwortet nie (f_mount-Fehler (3), unabhängig
// von der Verkabelung).
//
// HW-125-MODUL: VCC an 5V anschließen, NICHT 3,3V! Das Modul hat einen
// eigenen Onboard-Regler auf 3,3V für den Kartenchip. Mit 3,3V-Eingang hat
// der Regler zu wenig Spielraum (Dropout) -> Karte bekommt instabile
// Versorgung -> wechselnde f_mount-Fehler (1)/(13), obwohl Verkabelung und
// Formatierung korrekt sind. Am Board verifiziert: mit 5V läuft es stabil.
//
// Verdrahtung HW-125 <-> Heltec V2:
//   HW-125 VCC  -> 5V
//   HW-125 GND  -> GND
//   HW-125 MISO -> GPIO 19
//   HW-125 MOSI -> GPIO 27
//   HW-125 SCK  -> GPIO 5
//   HW-125 CS   -> GPIO 13
#define PIN_SPI_SCK      5
#define PIN_SPI_MISO    19
#define PIN_SPI_MOSI    27
#define PIN_SD_CS       13   // eigener Chip-Select (LoRa-CS = 18)

// --- LoRa-Chip-Select (board-intern, SX1276) ---
// LoRa wird (noch) nicht genutzt, sitzt aber am selben SPI-Bus wie die SD-Karte.
// Ohne explizites HIGH bleibt NSS undefiniert/LOW -> SX1276 haengt am Bus mit
// und stoert die SD-Kommunikation. Muss VOR jedem SD-Zugriff HIGH sein.
#define PIN_LORA_CS     18

// --- Onboard-Status-LED ---
#define PIN_LED         25

#endif // FLIGHT_PINS_H
