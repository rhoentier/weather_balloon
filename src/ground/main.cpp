// main.cpp — Bodenstation (Empfänger, Heltec-Board)
//
// PLATZHALTER. Empfängt später die LoRa-Telemetrie und gibt sie unverändert
// über USB-Seriell aus (Konzept §7.2). Aktuell nur Lebenszeichen.

#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("[ground] Wetterballon Bodenstation — Boot OK");
}

void loop() {
    delay(1000);
}
