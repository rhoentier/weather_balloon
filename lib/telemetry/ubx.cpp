// ubx.cpp — UBX-Flight-Mode-Logik (siehe ubx.h). Hardware-frei, nativ getestet.

#include "ubx.h"

namespace telemetry {

// 8-Bit-Fletcher-Prüfsumme (UBX-Standard) über [data, data+len).
void ubx_checksum(const uint8_t* data, size_t len, uint8_t& ck_a, uint8_t& ck_b) {
    ck_a = 0;
    ck_b = 0;
    for (size_t i = 0; i < len; ++i) {
        ck_a = static_cast<uint8_t>(ck_a + data[i]);
        ck_b = static_cast<uint8_t>(ck_b + ck_a);
    }
}

void build_cfg_nav5_airborne(uint8_t* out) {
    // Sync + Class/ID + Length(36)
    out[0] = 0xB5;
    out[1] = 0x62;
    out[2] = 0x06;  // Class: CFG
    out[3] = 0x24;  // ID: NAV5
    out[4] = 0x24;  // Length LSB = 36
    out[5] = 0x00;  // Length MSB

    // 36-Byte-Payload (UKHAS/AVA-Referenz für Dynamic Model 6, Airborne <1g):
    //   mask=0xFFFF (alle Felder), dynModel=6, fixMode=3 (auto 2D/3D), Rest Defaults.
    static const uint8_t payload[36] = {
        0xFF, 0xFF,             // mask: alle Parameter anwenden
        0x06,                   // dynModel = 6 (Airborne <1g)  ← der kritische Wert
        0x03,                   // fixMode = 3 (auto 2D/3D)
        0x00, 0x00, 0x00, 0x00, // fixedAlt
        0x10, 0x27, 0x00, 0x00, // fixedAltVar
        0x05,                   // minElev
        0x00,                   // drLimit
        0xFA, 0x00,             // pDop
        0xFA, 0x00,             // tDop
        0x64, 0x00,             // pAcc
        0x2C, 0x01,             // tAcc
        0x00,                   // staticHoldThresh
        0x00,                   // dgpsTimeOut
        0x00, 0x00, 0x00, 0x00, // reserved
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00              // reserved
    };
    for (size_t i = 0; i < 36; ++i) out[6 + i] = payload[i];

    // Prüfsumme über Class+ID+Length+Payload (Bytes 2..41 = 40 Bytes).
    uint8_t ck_a, ck_b;
    ubx_checksum(out + 2, 40, ck_a, ck_b);
    out[42] = ck_a;
    out[43] = ck_b;
}

bool is_cfg_nav5_ack(const uint8_t* buf, size_t len) {
    // Erwartet: B5 62 | 05 01 | 02 00 | 06 24 | CK_A CK_B  (10 Bytes)
    if (len < 10) return false;
    if (buf[0] != 0xB5 || buf[1] != 0x62) return false;
    if (buf[2] != 0x05 || buf[3] != 0x01) return false;  // ACK-ACK (nicht NAK 0x00)
    if (buf[6] != 0x06 || buf[7] != 0x24) return false;  // bestätigt CFG-NAV5

    // Prüfsumme über Bytes 2..7 (Class..Payload, 6 Bytes) verifizieren.
    uint8_t ck_a, ck_b;
    ubx_checksum(buf + 2, 6, ck_a, ck_b);
    return buf[8] == ck_a && buf[9] == ck_b;
}

} // namespace telemetry
