// gps_flightmode.cpp — siehe Header.

#include "gps_flightmode.h"
#include "ubx.h"

using namespace telemetry;

// Liest UBX-Bytes von `gps`, bis ein vollständiges ACK-ACK für CFG-NAV5
// erkannt wird oder das Timeout abläuft. Sucht den 0xB5-0x62-Sync und prüft
// dann die 10-Byte-ACK-Struktur über die getestete Logik.
static bool wait_for_ack(Stream& gps, uint32_t timeout_ms) {
    uint8_t buf[10];
    size_t have = 0;
    const uint32_t start = millis();

    while (millis() - start < timeout_ms) {
        while (gps.available()) {
            uint8_t b = static_cast<uint8_t>(gps.read());

            // Resynchronisieren: ein ACK beginnt immer mit 0xB5 0x62.
            if (have == 0) {
                if (b == 0xB5) buf[have++] = b;
            } else if (have == 1) {
                if (b == 0x62) buf[have++] = b;
                else have = (b == 0xB5) ? 1 : 0;  // evtl. neuer Sync-Start
            } else {
                buf[have++] = b;
                if (have == sizeof(buf)) {
                    if (is_cfg_nav5_ack(buf, have)) return true;
                    have = 0;  // war kein passendes ACK → weitersuchen
                }
            }
        }
        delay(2);
    }
    return false;
}

bool set_gps_flight_mode(Stream& gps, uint8_t retries, uint32_t ack_timeout_ms) {
    uint8_t msg[UBX_CFG_NAV5_LEN];
    build_cfg_nav5_airborne(msg);

    for (uint8_t attempt = 0; attempt < retries; ++attempt) {
        // Eingangspuffer leeren, damit altes NMEA das ACK nicht verdeckt.
        while (gps.available()) gps.read();

        gps.write(msg, UBX_CFG_NAV5_LEN);
        gps.flush();

        if (wait_for_ack(gps, ack_timeout_ms)) return true;
    }
    return false;
}
