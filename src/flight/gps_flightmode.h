// gps_flightmode.h — NEO-6M in den Höhenflug-Modus schalten (Hardware-Teil).
//
// Dünner Wrapper um die nativ getestete UBX-Logik (lib/telemetry/ubx). Hier
// passiert nur das, was echte Hardware braucht: Bytes über UART senden und
// auf das ACK warten. Muss am Boden verifiziert werden (TODO-Testreihenfolge).

#ifndef FLIGHT_GPS_FLIGHTMODE_H
#define FLIGHT_GPS_FLIGHTMODE_H

#include <Arduino.h>

// Sendet UBX-CFG-NAV5 (Dynamic Model 6) an das GPS auf `gps` und wartet auf
// die ACK-Bestätigung. Versucht es bis zu `retries` mal.
// Rückgabe: true = GPS hat bestätigt (Flight-Mode aktiv).
bool set_gps_flight_mode(Stream& gps, uint8_t retries = 3, uint32_t ack_timeout_ms = 1500);

#endif // FLIGHT_GPS_FLIGHTMODE_H
