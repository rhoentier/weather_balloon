// test_ubx.cpp — native Tests für die UBX-Flight-Mode-Logik.
//   pio test -e native
//
// Referenz: UKHAS / AVA HAB (https://ava.upuaut.net/?p=750) — die dort
// dokumentierte, im Feld bewährte CFG-NAV5-Sequenz für Dynamic Model 6.
// Wir testen, dass unser Code GENAU diese Bytes erzeugt — ein einziges
// falsches Byte würde den Höhenflug-Modus unwirksam machen.

#include <unity.h>
#include "ubx.h"
#include <cstring>

using namespace telemetry;

// Bekannt-korrekte 44-Byte-Sequenz (inkl. Checksumme 0x16 0xDC am Ende).
static const uint8_t REF_CFG_NAV5[UBX_CFG_NAV5_LEN] = {
    0xB5, 0x62, 0x06, 0x24, 0x24, 0x00, 0xFF, 0xFF, 0x06,
    0x03, 0x00, 0x00, 0x00, 0x00, 0x10, 0x27, 0x00, 0x00,
    0x05, 0x00, 0xFA, 0x00, 0xFA, 0x00, 0x64, 0x00, 0x2C,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0xDC
};

void setUp() {}
void tearDown() {}

// Die Checksumme über Class..Payload (Bytes 2..41) muss 0x16 / 0xDC ergeben.
void test_checksum_matches_reference() {
    uint8_t ck_a = 0, ck_b = 0;
    // ab Byte 2 (Class), 40 Bytes (Class+ID+Len+36 Payload), Sync ausgenommen
    ubx_checksum(REF_CFG_NAV5 + 2, 40, ck_a, ck_b);
    TEST_ASSERT_EQUAL_HEX8(0x16, ck_a);
    TEST_ASSERT_EQUAL_HEX8(0xDC, ck_b);
}

// Die gebaute Nachricht muss byte-genau der Referenz entsprechen.
void test_build_matches_reference_exactly() {
    uint8_t msg[UBX_CFG_NAV5_LEN];
    build_cfg_nav5_airborne(msg);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(REF_CFG_NAV5, msg, UBX_CFG_NAV5_LEN);
}

// Header + Class/ID + Längenfeld müssen stimmen (Sicherheitsnetz, falls die
// Referenzkonstante mal angefasst wird).
void test_build_header_and_class() {
    uint8_t msg[UBX_CFG_NAV5_LEN];
    build_cfg_nav5_airborne(msg);
    TEST_ASSERT_EQUAL_HEX8(0xB5, msg[0]);   // Sync 1
    TEST_ASSERT_EQUAL_HEX8(0x62, msg[1]);   // Sync 2
    TEST_ASSERT_EQUAL_HEX8(0x06, msg[2]);   // Class CFG
    TEST_ASSERT_EQUAL_HEX8(0x24, msg[3]);   // ID NAV5
    TEST_ASSERT_EQUAL_HEX8(0x24, msg[4]);   // Length LSB = 36
    TEST_ASSERT_EQUAL_HEX8(0x00, msg[5]);   // Length MSB
    TEST_ASSERT_EQUAL_HEX8(0x06, msg[8]);   // dynModel = 6 (Airborne <1g)
}

// Gültiges ACK-ACK für CFG-NAV5 wird erkannt.
void test_valid_ack_recognized() {
    // B5 62 | 05 01 (ACK-ACK) | 02 00 | 06 24 (ackt CFG-NAV5) | CK_A CK_B
    uint8_t ck_a = 0, ck_b = 0;
    uint8_t body[] = {0x05, 0x01, 0x02, 0x00, 0x06, 0x24};
    ubx_checksum(body, sizeof(body), ck_a, ck_b);
    uint8_t ack[] = {0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x24, ck_a, ck_b};
    TEST_ASSERT_TRUE(is_cfg_nav5_ack(ack, sizeof(ack)));
}

// ACK-NAK (0x05 0x00) darf NICHT als Erfolg gelten.
void test_nak_rejected() {
    uint8_t ck_a = 0, ck_b = 0;
    uint8_t body[] = {0x05, 0x00, 0x02, 0x00, 0x06, 0x24};  // NAK
    ubx_checksum(body, sizeof(body), ck_a, ck_b);
    uint8_t nak[] = {0xB5, 0x62, 0x05, 0x00, 0x02, 0x00, 0x06, 0x24, ck_a, ck_b};
    TEST_ASSERT_FALSE(is_cfg_nav5_ack(nak, sizeof(nak)));
}

// ACK für eine ANDERE Nachricht (nicht CFG-NAV5) darf nicht zählen.
void test_ack_for_other_message_rejected() {
    uint8_t ck_a = 0, ck_b = 0;
    uint8_t body[] = {0x05, 0x01, 0x02, 0x00, 0x06, 0x01};  // acked 06 01, nicht 06 24
    ubx_checksum(body, sizeof(body), ck_a, ck_b);
    uint8_t ack[] = {0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x01, ck_a, ck_b};
    TEST_ASSERT_FALSE(is_cfg_nav5_ack(ack, sizeof(ack)));
}

// Zu kurzer / kaputter Puffer wird sauber abgelehnt (kein Out-of-Bounds).
void test_truncated_buffer_rejected() {
    uint8_t buf[] = {0xB5, 0x62, 0x05, 0x01};
    TEST_ASSERT_FALSE(is_cfg_nav5_ack(buf, sizeof(buf)));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_checksum_matches_reference);
    RUN_TEST(test_build_matches_reference_exactly);
    RUN_TEST(test_build_header_and_class);
    RUN_TEST(test_valid_ack_recognized);
    RUN_TEST(test_nak_rejected);
    RUN_TEST(test_ack_for_other_message_rejected);
    RUN_TEST(test_truncated_buffer_rejected);
    return UNITY_END();
}
