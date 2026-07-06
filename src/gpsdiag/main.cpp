// gpsdiag/main.cpp — TEMPORÄRE GPS-Diagnose-Firmware (kein Flugcode!)
//
// Zweck: klären, warum der NEO-6M unter freiem Himmel keinen Fix (und nicht mal
// eine UTC-Zeit) bekommt. Diese Firmware parst NICHTS weg, sondern reicht die
// rohen NMEA-Sätze des GPS 1:1 auf den Serial-Monitor durch und zählt nebenbei
// die empfangenen Bytes.
//
// So liest man das Ergebnis:
//   • Es kommen $GPGSV-Sätze mit SNR-Werten > 0 (die Zahlen am Satz-Ende)
//       → das Modul SIEHT Satelliten. Antenne HF-ok. Kein-Fix liegt dann an
//         Kaltstart/leerer Backup-Batterie (Almanach) → länger warten.
//   • Es kommen nur leere Sätze ($GPRMC,,V,,,…  /  $GPGSV,…,,,, ohne SNR)
//       → Modul empfängt nichts vom Himmel. Antenne/HF-Teil defekt.
//   • Byte-Zähler bleibt 0
//       → gar keine Daten vom Modul (Verkabelung/Stromversorgung) — sollte hier
//         NICHT passieren, im letzten Flug-Log kamen ja Sätze an.
//
// Flashen:  pio run -e gpsdiag -t upload   &&   pio device monitor
// Danach zurück auf die Flug-Firmware:  pio run -e flight -t upload

#include <Arduino.h>
#include "../flight/pins.h"   // gemeinsame Pin-Definitionen (GPS_RX/TX, Vext, LED)

static uint32_t g_bytes_total = 0;   // seit Boot empfangene GPS-Bytes
static uint32_t g_last_report = 0;   // millis() des letzten 5-s-Reports

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println("[gpsdiag] === GPS-Rohdaten-Diagnose (NEO-6M) ===");

    // Vext AN — versorgt auf dem V2 die 3,3-V-Peripherie. Schadet nicht, falls
    // das GPS an der dauerversorgten Schiene hängt; stellt sicher, dass es Saft
    // hat, falls doch an Vext.
    pinMode(PIN_VEXT, OUTPUT);
    digitalWrite(PIN_VEXT, LOW);          // LOW = Vext AN
    delay(100);

    // GPS-UART exakt wie in der Flug-Firmware (gekreuzt, 9600 Baud).
    Serial2.begin(GPS_BAUD, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
    Serial.print("[gpsdiag] Serial2 @");
    Serial.print(GPS_BAUD);
    Serial.print(" Baud, RX=GPIO"); Serial.print(PIN_GPS_RX);
    Serial.print(", TX=GPIO"); Serial.println(PIN_GPS_TX);
    Serial.println("[gpsdiag] Flight-Mode NICHT gesetzt (Standardmodus).");
    Serial.println("[gpsdiag] --- ab hier rohe NMEA-Saetze ---");
}

void loop() {
    // Jedes vom GPS empfangene Byte 1:1 durchreichen und mitzählen.
    while (Serial2.available()) {
        char c = static_cast<char>(Serial2.read());
        Serial.write(c);
        g_bytes_total++;
    }

    // Alle 5 s eine Zusammenfassung, damit man auch bei Stille etwas sieht.
    uint32_t now = millis();
    if (now - g_last_report >= 5000) {
        g_last_report = now;
        Serial.print("\n[gpsdiag] t=");
        Serial.print(now / 1000);
        Serial.print("s  Bytes gesamt=");
        Serial.println(g_bytes_total);
    }
}
