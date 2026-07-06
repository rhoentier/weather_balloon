// main.cpp — Flug-Einheit (Heltec WiFi LoRa 32 V2)
//
// Stand: GPS-Pipeline. setup() setzt GPS-Flight-Mode + initialisiert SD.
// loop() füttert den GPS-UART laufend an TinyGPSPlus (gps_reader) und schreibt
// einmal pro Sekunde eine Telemetrie-CSV-Zeile aus dem aktuellen GPS-Zustand
// (gps_fill -> TelemetryRecord -> Flugphase -> csv_row) auf Serial und microSD.

#include <Arduino.h>
#include <Wire.h>
#include "pins.h"
#include "gps_flightmode.h"
#include "sd_log.h"
#include "gps_reader.h"

#include "record.h"
#include "flight_phase.h"
#include "oled.h"
#include "bmp_sensor.h"
#include "ds18b20.h"
#include "uv_sensor.h"
#include "mpu_sensor.h"
#include "display_status.h"

using namespace telemetry;

static TelemetryRecord     g_rec;
static FlightPhaseDetector g_detector;
static bool g_sd_ok       = false;  // Ergebnis von sd_log_begin(), fürs Display
static bool g_bmp_ok      = false;  // Ergebnis von bmp_begin(), fürs Display
static bool g_ds_ok       = false;  // Ergebnis von ds_begin(), fürs Display
static bool g_uv_ok       = false;  // Ergebnis von uv_begin(), fürs Display
static bool g_mpu_ok      = false;  // Ergebnis von mpu_begin(), fürs Display
static bool g_oled_active = true;   // Display läuft, bis Timeout ODER Aufstieg
static uint32_t g_last_oled_ms = 0;              // letzte OLED-Aktualisierung
static const uint32_t OLED_REFRESH_MS = 500;     // OLED höchstens alle 500 ms neu zeichnen
static const uint32_t OLED_ON_MS = 1000UL * 60 * 5;  // Display am Boden 5 min an, dann aus
static uint32_t g_last_log_ms = 0;               // letzte CSV-Zeile geschrieben
static const uint32_t LOG_INTERVAL_MS = 1000;    // eine CSV-Zeile pro Sekunde (1 Hz)

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println("[flight] Wetterballon Flug-Einheit — Boot");

    // GPS an UART2 starten (NEO-6M, 9600 Baud, RX23/TX17 lt. Pin-Spec).
    Serial2.begin(GPS_BAUD, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
    Serial.println("[flight] GPS UART2 @9600 gestartet");

    // KRITISCH: Höhenflug-Modus setzen, sonst GPS-Abschaltung > ~18 km.
    Serial.println("[flight] Setze GPS Flight-Mode (Dynamic Model 6)...");
    if (set_gps_flight_mode(Serial2)) {
        Serial.println("[flight] >>> GPS Flight-Mode BESTAETIGT (ACK) <<<");
    } else {
        Serial.println("[flight] !!! GPS Flight-Mode NICHT bestaetigt — pruefen! !!!");
    }

    // LoRa (SX1276) deselektieren: sitzt am selben SPI-Bus wie die SD-Karte.
    // Ohne dieses HIGH bleibt NSS floating/LOW und der Chip haengt am Bus mit,
    // was die SD-Kommunikation stoert (LoRa wird in dieser Firmware nicht genutzt).
    pinMode(PIN_LORA_CS, OUTPUT);
    digitalWrite(PIN_LORA_CS, HIGH);

    // microSD initialisieren (Logging optional — Betrieb läuft auch ohne).
    g_sd_ok = sd_log_begin();

    // Vext AN (versorgt OLED *und* die I²C-Sensoren) und danach den I²C-Bus
    // EINMALIG auf die V2-OLED-Pins 4/15 aufsetzen. Grund: Auf dem Heltec V2
    // sind die Wire-Default-Pins 21/22 (= Vext bzw. DS18B20), NICHT der
    // Sensor-Bus. ESP32-Wire.begin() ist ein No-op, sobald der Bus einmal
    // läuft — wer zuerst begin() ruft, legt die Pins fest. Also hier zentral
    // und VOR dem ersten I²C-Zugriff (BMP/OLED), sonst würde Adafruit_BMP280
    // den Bus auf 21/22 initialisieren und OLED + Sensor gingen leer aus.
    pinMode(PIN_VEXT, OUTPUT);
    digitalWrite(PIN_VEXT, LOW);          // LOW = Vext AN
    delay(50);                            // Rail + Sensoren stabilisieren
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL); // 4 / 15

    g_bmp_ok = bmp_begin();
    if (g_bmp_ok) {
        Serial.print("[flight] BMP280 gefunden (0x76), Höhen-Referenz = ");
        Serial.print(bmp_reference_hpa(), 2);
        Serial.println(" hPa (Startort)");
    } else {
        Serial.println("[flight] !!! BMP280 NICHT gefunden (0x76) !!!");
    }

    // DS18B20 (Außentemperatur, 1-Wire an GPIO22).
    g_ds_ok = ds_begin();
    if (g_ds_ok) {
        Serial.println("[flight] DS18B20 gefunden (1-Wire)");
    } else {
        Serial.println("[flight] !!! DS18B20 NICHT gefunden (1-Wire) !!!");
    }

    // GUVA-S12SD (UV, analog an GPIO36/ADC1). Kein Presence-Check möglich
    // (reiner Analog-Pin) → uv_begin() konfiguriert nur den ADC.
    g_uv_ok = uv_begin();
    Serial.println("[flight] GUVA-S12SD UV: ADC an GPIO36 konfiguriert (ADC-Rohwert 0..4095)");

    // MPU-6050 (IMU, I²C an 0x68 — selber Bus wie BMP280/OLED).
    g_mpu_ok = mpu_begin();
    if (g_mpu_ok) {
        Serial.println("[flight] MPU-6050 gefunden (0x68)");
    } else {
        Serial.println("[flight] !!! MPU-6050 NICHT gefunden (0x68) !!!");
    }

    // CSV-Kopfzeile einmal auf Serial ausgeben (Orientierung im Monitor).
    Serial.println(csv_header().c_str());

    // OLED starten (nutzt den oben gesetzten I²C-Bus) — zeigt den Boden-Check.
    oled_begin();
}

void loop() {
    // GPS laufend füttern (nicht-blockierend): TinyGPSPlus sammelt + parst.
    gps_feed(Serial2);

    // Zeitgesteuert: einmal pro Sekunde eine CSV-Zeile aus dem aktuellen Zustand.
    if (millis() - g_last_log_ms >= LOG_INTERVAL_MS) {
        g_last_log_ms = millis();

        g_rec.t_ms = millis();
        gps_fill(g_rec);   // GPS-Felder aus TinyGPSPlus in den Record
        bmp_read(g_rec);   // BMP280 lesen VOR Phasenerkennung (Höhe für Fallback)

        // Phasenerkennung: GPS-Höhe bevorzugt, BMP280-Höhe als Fallback.
        if (g_rec.has_fix) {
            g_rec.phase = g_detector.update(g_rec.alt_gps_m, g_rec.t_ms);
        } else if (g_rec.has_bmp) {
            g_rec.phase = g_detector.update(g_rec.alt_baro_m, g_rec.t_ms);
        } else {
            g_rec.phase = g_detector.update_no_altitude(g_rec.t_ms);
        }
        ds_update(g_rec);   // non-blocking: übernimmt Wert nur, wenn Wandlung fertig
        uv_read(g_rec);
        mpu_read(g_rec);

        String csv = csv_row(g_rec).c_str();
        Serial.println(csv);
        sd_log(csv);
    }

    // Boden-Check periodisch anzeigen — bewusst UNABHÄNGIG von eingehenden
    // GPS-Daten, damit auch ein stummes GPS-Modul als "GPS: --" sichtbar wird
    // (der Silent-Fall wäre sonst nie erreichbar). Gedrosselt gegen I²C-Spam.
    if (g_oled_active && (millis() - g_last_oled_ms >= OLED_REFRESH_MS)) {
        g_last_oled_ms = millis();
        // Abschalten, sobald 5 min um sind ODER der Aufstieg begonnen hat —
        // was zuerst kommt. In der Praxis greift meist der Timeout.
        if (millis() < OLED_ON_MS && g_detector.phase() == Phase::PreFlight) {
            DisplayState ds;
            ds.gps   = gps_display_state();
            ds.sats  = g_rec.sats;
            ds.sd_ok = g_sd_ok;
            ds.bmp_ok = g_bmp_ok;
            ds.ds18b20_ok = g_ds_ok;
            ds.uv_ok = g_uv_ok;
            ds.mpu_ok = g_mpu_ok;
            ds.phase = g_detector.phase();
            oled_show(status_lines(ds));
        } else {
            oled_off();          // Timeout oder Flugstart -> Display aus (einmalig)
            g_oled_active = false;
        }
    }
}
