// test_bmp280.cpp — native Tests für die BMP280-Umrechnung.
//   pio test -e native
//
// Referenzwerte verifiziert aus dem Datenblatt-C-Code (BST-BMP280-DS001
// Rev 1.26, 3.11.3), NICHT aus dem Gedächtnis. Kalibriersatz + Ergebnisse:
//   adc_T=519888 -> t_fine=128422, T=2508 (25,08 °C)
//   adc_P=415148 -> 100656 Pa (1006,56 hPa)

#include <unity.h>
#include "bmp280.h"

using namespace telemetry;

void setUp() {}
void tearDown() {}

// Klassischer, in mehreren Bibliotheken zitierter Kalibriersatz.
static Bmp280Calib ref_calib() {
    Bmp280Calib c;
    c.dig_T1 = 27504;  c.dig_T2 = 26435;  c.dig_T3 = -1000;
    c.dig_P1 = 36477;  c.dig_P2 = -10685; c.dig_P3 = 3024;
    c.dig_P4 = 2855;   c.dig_P5 = 140;    c.dig_P6 = -7;
    c.dig_P7 = 15500;  c.dig_P8 = -14600; c.dig_P9 = 6000;
    return c;
}

// Temperatur gegen den verifizierten Referenzwert (25,08 °C) + t_fine.
void test_temperature_reference() {
    Bmp280Calib c = ref_calib();
    int32_t t_fine = 0;
    int32_t T = bmp280_compensate_temperature(519888, c, t_fine);
    TEST_ASSERT_EQUAL_INT32(128422, t_fine);
    TEST_ASSERT_EQUAL_INT32(2508, T);   // 25,08 °C
}

// Vorzeichen-Falle: negative Kalibrierkonstanten müssen als signed wirken.
// Würde dig_T3=-1000 fälschlich als uint16 (65536-1000) gelesen, käme ein
// grob anderer t_fine heraus -> dieser Test schlägt dann fehl.
void test_temperature_signed_calib() {
    Bmp280Calib c = ref_calib();
    int32_t t_fine = 0;
    bmp280_compensate_temperature(519888, c, t_fine);
    TEST_ASSERT_EQUAL_INT32(128422, t_fine);
}

// Druck gegen den verifizierten Referenzwert (100656 Pa). Nutzt t_fine aus der
// Temperatur-Kompensation — Reihenfolge wie im echten loop().
void test_pressure_reference() {
    Bmp280Calib c = ref_calib();
    int32_t t_fine = 0;
    bmp280_compensate_temperature(519888, c, t_fine);   // füllt t_fine
    uint32_t p = bmp280_compensate_pressure(415148, c, t_fine);
    TEST_ASSERT_EQUAL_UINT32(100656, p);   // 1006,56 hPa
}

// Höhe: gleicher Druck wie Referenzdruck -> 0 m.
void test_altitude_zero_at_sea_level() {
    float h = bmp280_altitude_m(101325.0f, 101325.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, h);
}

// Höhe: verifizierter Standardpunkt (p=89876 Pa, QNH=101325 Pa) -> ~1000 m.
void test_altitude_reference_point() {
    float h = bmp280_altitude_m(89876.0f, 101325.0f);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 1000.0f, h);   // 1000,02 m ± 1 m
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_temperature_reference);
    RUN_TEST(test_temperature_signed_calib);
    RUN_TEST(test_pressure_reference);
    RUN_TEST(test_altitude_zero_at_sea_level);
    RUN_TEST(test_altitude_reference_point);
    return UNITY_END();
}
