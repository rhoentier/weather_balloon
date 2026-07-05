// oled.cpp — siehe Header. U8g2, SSD1306 128x64, HW-I2C.
#include "oled.h"
#include "pins.h"
#include <Arduino.h>
#include <U8g2lib.h>

// V2-Pinbelegung (verifiziert): reset=16, clock=SCL(15), data=SDA(4).
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C
    g_u8g2(U8G2_R0, /*reset=*/16, /*clock=*/PIN_I2C_SCL, /*data=*/PIN_I2C_SDA);

void oled_begin() {
    pinMode(PIN_VEXT, OUTPUT);
    digitalWrite(PIN_VEXT, LOW);   // Vext AN (versorgt OLED/Sensoren)
    delay(50);                     // Rail + Display kurz stabilisieren
    g_u8g2.begin();
    g_u8g2.setFont(u8g2_font_6x12_tf);
}

void oled_show(const std::vector<std::string>& lines) {
    g_u8g2.clearBuffer();
    int y = 12;                    // erste Grundlinie (Fonthöhe ~12 px)
    for (const auto& line : lines) {
        g_u8g2.drawStr(0, y, line.c_str());
        y += 13;                   // Zeilenabstand
    }
    g_u8g2.sendBuffer();
}

void oled_off() {
    g_u8g2.setPowerSave(1);        // Display-Controller schlafen legen
    digitalWrite(PIN_VEXT, HIGH);  // Vext AUS (Strom sparen im Flug)
}
