// sd_log.h — microSD-Logging der Telemetrie-CSV (Hardware-Teil).
//
// Dünner Wrapper um die Arduino-SD-Bibliothek (SPI, CS an PIN_SD_CS=13, Bus mit
// LoRa geteilt). Feste Datei /flight.csv im Append-Modus: bei Neustart wird
// weitergeschrieben. Header nur bei neuer/leerer Datei. Am Board zu verifizieren.

#ifndef FLIGHT_SD_LOG_H
#define FLIGHT_SD_LOG_H

#include <Arduino.h>

// Initialisiert die SD-Karte (SPI, CS = PIN_SD_CS). Rückgabe: true = bereit.
// Legt bei neuer/leerer /flight.csv die CSV-Kopfzeile (csv_header()) an.
bool sd_log_begin();

// Hängt eine CSV-Zeile (ohne Zeilenende) an /flight.csv an.
// Bei nicht initialisierter/fehlerhafter Karte: no-op.
void sd_log(const String& line);

#endif // FLIGHT_SD_LOG_H
