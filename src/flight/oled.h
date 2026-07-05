// oled.h — board-internes SSD1306-OLED (Heltec V2). Hardware-Teil.
//
// Zeigt den „Startklar?"-Check am Boden. Formatierung der Zeilen kommt aus
// lib/telemetry/display_status (nativ getestet); hier nur Init/Zeichnen/Abschalten.
// Pins: clock=SCL(15), data=SDA(4), reset=16. Versorgung via Vext (GPIO21, LOW=an).
#ifndef FLIGHT_OLED_H
#define FLIGHT_OLED_H

#include <string>
#include <vector>

// Vext an (GPIO21 LOW), kurz warten, U8g2 initialisieren.
void oled_begin();

// Zeilen zeichnen (eine pro Textzeile, oben beginnend).
void oled_show(const std::vector<std::string>& lines);

// Display in Power-Save + Vext aus (GPIO21 HIGH). Für Flugstart.
void oled_off();

#endif // FLIGHT_OLED_H
