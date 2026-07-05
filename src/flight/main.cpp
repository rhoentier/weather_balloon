// main.cpp — Flug-Einheit (Heltec WiFi LoRa 32 V2)
//
// Stand: GPS-Pipeline. setup() setzt GPS-Flight-Mode + initialisiert SD.
// loop() liest den GPS-UART, baut pro GGA-Satz eine Telemetrie-CSV-Zeile
// (parse_gga -> TelemetryRecord -> Flugphase -> csv_row) und schreibt sie auf
// Serial und microSD. Testbare Logik lebt in lib/telemetry (nativ getestet).

#include <Arduino.h>
#include <Wire.h>
#include "pins.h"
#include "gps_flightmode.h"
#include "sd_log.h"

#include "line_assembler.h"
#include "gga.h"
#include "record.h"
#include "flight_phase.h"
#include "oled.h"
#include "bme_sensor.h"
#include "display_status.h"

using namespace telemetry;

static LineAssembler       g_asm;
static TelemetryRecord     g_rec;
static FlightPhaseDetector g_detector;
static bool g_sd_ok       = false;  // Ergebnis von sd_log_begin(), fürs Display
static bool g_bme_ok      = false;  // Ergebnis von bme_begin(), fürs Display
static bool g_gps_seen    = false;  // schon je eine GGA geparst? (GPS-Stufe)
static bool g_oled_active = true;   // Display läuft, bis PreFlight verlassen wird
static uint32_t g_last_oled_ms = 0;              // letzte OLED-Aktualisierung
static const uint32_t OLED_REFRESH_MS = 500;     // OLED höchstens alle 500 ms neu zeichnen

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

    // microSD initialisieren (Logging optional — Betrieb läuft auch ohne).
    g_sd_ok = sd_log_begin();

    // Vext AN (versorgt OLED *und* die I²C-Sensoren) und danach den I²C-Bus
    // EINMALIG auf die V2-OLED-Pins 4/15 aufsetzen. Grund: Auf dem Heltec V2
    // sind die Wire-Default-Pins 21/22 (= Vext bzw. DS18B20), NICHT der
    // Sensor-Bus. ESP32-Wire.begin() ist ein No-op, sobald der Bus einmal
    // läuft — wer zuerst begin() ruft, legt die Pins fest. Also hier zentral
    // und VOR dem ersten I²C-Zugriff (BME/OLED), sonst würde Adafruit_BME280
    // den Bus auf 21/22 initialisieren und OLED + Sensor gingen leer aus.
    pinMode(PIN_VEXT, OUTPUT);
    digitalWrite(PIN_VEXT, LOW);          // LOW = Vext AN
    delay(50);                            // Rail + Sensoren stabilisieren
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL); // 4 / 15

    g_bme_ok = bme_begin();
    Serial.println(g_bme_ok ? "[flight] BME280 gefunden (0x76)"
                             : "[flight] !!! BME280 NICHT gefunden (0x76) !!!");

    // CSV-Kopfzeile einmal auf Serial ausgeben (Orientierung im Monitor).
    Serial.println(csv_header().c_str());

    // OLED starten (nutzt den oben gesetzten I²C-Bus) — zeigt den Boden-Check.
    oled_begin();
}

void loop() {
    while (Serial2.available()) {
        char c = static_cast<char>(Serial2.read());
        std::string line;
        if (!g_asm.push(c, line)) continue;

        GpsFix fix;
        if (!parse_gga(line, fix)) continue;   // Nicht-GGA / kaputt -> überspringen

        g_gps_seen = true;

        g_rec.t_ms      = millis();
        g_rec.has_utc   = fix.has_utc;
        g_rec.utc_h     = fix.utc_h;
        g_rec.utc_min   = fix.utc_min;
        g_rec.utc_s     = fix.utc_s;
        g_rec.has_fix     = fix.has_fix;
        g_rec.fix_quality = fix.fix_quality;
        g_rec.lat       = fix.lat;
        g_rec.lon       = fix.lon;
        g_rec.alt_gps_m = fix.alt_gps_m;
        g_rec.sats      = fix.sats;

        if (fix.has_fix) {
            g_rec.phase = g_detector.update(fix.alt_gps_m, g_rec.t_ms);
        } else {
            g_rec.phase = g_detector.phase();  // ohne Fix letzte Phase halten
        }

        bme_read(g_rec);

        String csv = csv_row(g_rec).c_str();
        Serial.println(csv);
        sd_log(csv);
    }

    // Boden-Check periodisch anzeigen — bewusst UNABHÄNGIG von eingehenden
    // GPS-Daten, damit auch ein stummes GPS-Modul als "GPS: --" sichtbar wird
    // (der Silent-Fall wäre sonst nie erreichbar). Gedrosselt gegen I²C-Spam.
    if (g_oled_active && (millis() - g_last_oled_ms >= OLED_REFRESH_MS)) {
        g_last_oled_ms = millis();
        if (g_detector.phase() == Phase::PreFlight) {
            DisplayState ds;
            ds.gps   = g_rec.has_fix ? GpsDisp::Fix
                     : g_gps_seen    ? GpsDisp::Waiting
                                     : GpsDisp::Silent;
            ds.sats  = g_rec.sats;
            ds.sd_ok = g_sd_ok;
            ds.bme_ok = g_bme_ok;
            ds.phase = g_detector.phase();
            oled_show(status_lines(ds));
        } else {
            oled_off();          // Flug begonnen -> Display aus (einmalig)
            g_oled_active = false;
        }
    }
}
