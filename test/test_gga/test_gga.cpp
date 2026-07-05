// test_gga.cpp — native Tests für den NMEA-GGA-Parser.
//   pio test -e native
//
// Referenzsätze mit echten, nachgerechneten Werten. NMEA-Koordinaten sind
// ddmm.mmmm -> Dezimalgrad = dd + mm.mmmm/60.

#include <unity.h>
#include "gga.h"
#include <string>

using namespace telemetry;

void setUp() {}
void tearDown() {}

// Referenz-GGA mit gültiger Checksumme (*47 stimmt für diese Zeile).
static const char* GGA_VALID =
    "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47";

// Gültige Zeile -> true.
void test_valid_gga_accepted() {
    GpsFix fix;
    TEST_ASSERT_TRUE(parse_gga(GGA_VALID, fix));
}

// Verfälschte Checksumme (*48 statt *47) -> false.
void test_bad_checksum_rejected() {
    GpsFix fix;
    std::string bad = GGA_VALID;
    bad[bad.size() - 1] = '8';  // *47 -> *48
    TEST_ASSERT_FALSE(parse_gga(bad, fix));
}

// Nicht-GGA-Satz (RMC) -> false.
void test_non_gga_rejected() {
    GpsFix fix;
    // RMC-Zeile mit gültiger Checksumme, aber kein GGA.
    TEST_ASSERT_FALSE(parse_gga(
        "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A",
        fix));
}

// Talker-Variante $GNGGA (Multi-GNSS) wird als GGA erkannt.
void test_gnss_talker_accepted() {
    GpsFix fix;
    // Gleiche Nutzlast wie GGA_VALID, aber Talker GN statt GP -> andere Checksumme.
    // Checksumme für "GNGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,," = 0x59
    TEST_ASSERT_TRUE(parse_gga(
        "$GNGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*59",
        fix));
}

// Leere Zeile -> false, kein Absturz.
void test_empty_rejected() {
    GpsFix fix;
    TEST_ASSERT_FALSE(parse_gga("", fix));
}

// Gültiger Fix (Qualität 1) -> has_fix true, Höhe + Sats korrekt.
void test_fields_with_fix() {
    GpsFix fix;
    TEST_ASSERT_TRUE(parse_gga(GGA_VALID, fix));
    TEST_ASSERT_TRUE(fix.has_fix);
    TEST_ASSERT_EQUAL_UINT8(8, fix.sats);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 545.4f, fix.alt_gps_m);
}

// Fix-Qualität 0 -> gültige Zeile (true), aber has_fix false.
// Checksumme nachgerechnet: XOR("GPGGA,123519,4807.038,N,01131.000,E,0,08,0.9,545.4,M,46.9,M,,") = 0x46
void test_quality_zero_no_fix() {
    GpsFix fix;
    fix.has_fix = true;  // Vorbedingung setzen, damit der Assert wirklich prüft
    TEST_ASSERT_TRUE(parse_gga(
        "$GPGGA,123519,4807.038,N,01131.000,E,0,08,0.9,545.4,M,46.9,M,,*46",
        fix));
    TEST_ASSERT_FALSE(fix.has_fix);
}

// Rohe Fix-Qualität wird erhalten: Feld [6] = "1" (Standard-GPS-Fix).
void test_fix_quality_one() {
    GpsFix fix;
    TEST_ASSERT_TRUE(parse_gga(GGA_VALID, fix));
    TEST_ASSERT_EQUAL_UINT8(1, fix.fix_quality);
}

// Rohe Fix-Qualität 0 (kein Fix) bleibt als 0 erhalten (has_fix false).
void test_fix_quality_zero() {
    GpsFix fix;
    fix.fix_quality = 9;  // vorbelegen, muss überschrieben werden
    TEST_ASSERT_TRUE(parse_gga(
        "$GPGGA,123519,4807.038,N,01131.000,E,0,08,0.9,545.4,M,46.9,M,,*46",
        fix));
    TEST_ASSERT_EQUAL_UINT8(0, fix.fix_quality);
}

// 4807.038,N -> 48 + 07.038/60 = 48.1173 ; 01131.000,E -> 11 + 31/60 = 11.5167.
void test_coords_north_east() {
    GpsFix fix;
    TEST_ASSERT_TRUE(parse_gga(GGA_VALID, fix));
    TEST_ASSERT_DOUBLE_WITHIN(1e-4, 48.1173, fix.lat);
    TEST_ASSERT_DOUBLE_WITHIN(1e-4, 11.5167, fix.lon);
}

// Süd/West -> negative Vorzeichen.
// Checksumme nachgerechnet: XOR("GPGGA,123519,4807.038,S,01131.000,W,1,08,0.9,545.4,M,46.9,M,,") = 0x48
void test_coords_south_west() {
    GpsFix fix;
    TEST_ASSERT_TRUE(parse_gga(
        "$GPGGA,123519,4807.038,S,01131.000,W,1,08,0.9,545.4,M,46.9,M,,*48",
        fix));
    TEST_ASSERT_DOUBLE_WITHIN(1e-4, -48.1173, fix.lat);
    TEST_ASSERT_DOUBLE_WITHIN(1e-4, -11.5167, fix.lon);
}

// Mitten abgeschnittene GGA-Zeile (kein '*HH') -> false, kein Absturz.
void test_truncated_line_rejected() {
    GpsFix fix;
    TEST_ASSERT_FALSE(parse_gga("$GPGGA,123519,4807.03", fix));
}

// GGA mit gültiger Checksumme, aber zu wenig Feldern -> false.
// Checksumme nachgerechnet: XOR("GPGGA,123519,4807.038,N") = 0x27
void test_too_few_fields_rejected() {
    GpsFix fix;
    TEST_ASSERT_FALSE(parse_gga("$GPGGA,123519,4807.038,N*27", fix));
}

// UTC aus GGA-Feld [1] "123519" -> 12:35:19.
void test_utc_parsed() {
    GpsFix fix;
    TEST_ASSERT_TRUE(parse_gga(GGA_VALID, fix));
    TEST_ASSERT_TRUE(fix.has_utc);
    TEST_ASSERT_EQUAL_UINT8(12, fix.utc_h);
    TEST_ASSERT_EQUAL_UINT8(35, fix.utc_min);
    TEST_ASSERT_EQUAL_UINT8(19, fix.utc_s);
}

// Führende Nullen: "080509" -> 8:5:9 (nicht an der 0 stolpern).
// Checksumme für "GPGGA,080509,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,," = 0x4E
void test_utc_leading_zeros() {
    GpsFix fix;
    TEST_ASSERT_TRUE(parse_gga(
        "$GPGGA,080509,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*4E",
        fix));
    TEST_ASSERT_TRUE(fix.has_utc);
    TEST_ASSERT_EQUAL_UINT8(8, fix.utc_h);
    TEST_ASSERT_EQUAL_UINT8(5, fix.utc_min);
    TEST_ASSERT_EQUAL_UINT8(9, fix.utc_s);
}

// UTC gültig auch OHNE Positions-Fix (Fix-Qualität 0).
// Zeile identisch zu test_quality_zero_no_fix (*46).
void test_utc_without_fix() {
    GpsFix fix;
    TEST_ASSERT_TRUE(parse_gga(
        "$GPGGA,123519,4807.038,N,01131.000,E,0,08,0.9,545.4,M,46.9,M,,*46",
        fix));
    TEST_ASSERT_FALSE(fix.has_fix);
    TEST_ASSERT_TRUE(fix.has_utc);
    TEST_ASSERT_EQUAL_UINT8(12, fix.utc_h);
}

// Leeres UTC-Feld [1] -> has_utc false.
// Checksumme für "GPGGA,,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,," = 0x4A
void test_utc_empty_field() {
    GpsFix fix;
    fix.has_utc = true;  // Vorbedingung, damit der Assert wirklich prüft
    TEST_ASSERT_TRUE(parse_gga(
        "$GPGGA,,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*4A",
        fix));
    TEST_ASSERT_FALSE(fix.has_utc);
}

// Zu kurzes UTC-Feld (< 6 Ziffern) -> has_utc false.
// Checksumme für "GPGGA,1235,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,," = 0x4F
void test_utc_too_short() {
    GpsFix fix;
    fix.has_utc = true;
    TEST_ASSERT_TRUE(parse_gga(
        "$GPGGA,1235,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*4F",
        fix));
    TEST_ASSERT_FALSE(fix.has_utc);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_valid_gga_accepted);
    RUN_TEST(test_bad_checksum_rejected);
    RUN_TEST(test_non_gga_rejected);
    RUN_TEST(test_gnss_talker_accepted);
    RUN_TEST(test_empty_rejected);
    RUN_TEST(test_fields_with_fix);
    RUN_TEST(test_quality_zero_no_fix);
    RUN_TEST(test_fix_quality_one);
    RUN_TEST(test_fix_quality_zero);
    RUN_TEST(test_coords_north_east);
    RUN_TEST(test_coords_south_west);
    RUN_TEST(test_truncated_line_rejected);
    RUN_TEST(test_too_few_fields_rejected);
    RUN_TEST(test_utc_parsed);
    RUN_TEST(test_utc_leading_zeros);
    RUN_TEST(test_utc_without_fix);
    RUN_TEST(test_utc_empty_field);
    RUN_TEST(test_utc_too_short);
    return UNITY_END();
}
