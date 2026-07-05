// test_display_status.cpp — native Tests für die OLED-Statuszeilen.
//   pio test -e native
// Reine Textformatierung (hardware-frei). Prüft Layout-Kontrakt + GPS-Stufen.
#include <unity.h>
#include "display_status.h"
#include <string>
#include <vector>

using namespace telemetry;

void setUp() {}
void tearDown() {}

static DisplayState base() {
    return DisplayState{ GpsDisp::Silent, 0, false, Phase::PreFlight,
                          false, false, false, false };
}

void test_layout_has_four_lines_in_order() {
    auto l = status_lines(base());
    TEST_ASSERT_EQUAL_INT(5, (int)l.size());
    TEST_ASSERT_EQUAL_STRING("Wetterballon", l[0].c_str());
    TEST_ASSERT_TRUE(l[1].rfind("GPS:", 0) == 0);
    TEST_ASSERT_TRUE(l[2].rfind("SD:", 0) == 0);
    TEST_ASSERT_TRUE(l[3].rfind("Phase:", 0) == 0);
    TEST_ASSERT_TRUE(l[4].rfind("B:", 0) == 0);
}

void test_gps_silent() {
    auto s = base(); s.gps = GpsDisp::Silent;
    TEST_ASSERT_EQUAL_STRING("GPS: --", status_lines(s)[1].c_str());
}

void test_gps_waiting() {
    auto s = base(); s.gps = GpsDisp::Waiting;
    TEST_ASSERT_EQUAL_STRING("GPS: warte...", status_lines(s)[1].c_str());
}

void test_gps_fix_shows_sat_count() {
    auto s = base(); s.gps = GpsDisp::Fix; s.sats = 7;
    TEST_ASSERT_EQUAL_STRING("GPS: Fix 7 Sat", status_lines(s)[1].c_str());
}

void test_sd_ok_and_missing() {
    auto s = base(); s.sd_ok = true;
    TEST_ASSERT_EQUAL_STRING("SD: ok", status_lines(s)[2].c_str());
    s.sd_ok = false;
    TEST_ASSERT_EQUAL_STRING("SD: --", status_lines(s)[2].c_str());
}

void test_phase_uses_to_string() {
    auto s = base(); s.phase = Phase::Ascent;
    std::string expected = std::string("Phase: ") + to_string(Phase::Ascent);
    TEST_ASSERT_EQUAL_STRING(expected.c_str(), status_lines(s)[3].c_str());
}

// Alle vier Sensoren ok -> alle vier Kürzel zeigen "ok".
void test_sensors_all_ok() {
    auto s = base();
    s.bmp_ok = true; s.mpu_ok = true; s.ds18b20_ok = true; s.uv_ok = true;
    TEST_ASSERT_EQUAL_STRING("B:ok M:ok D:ok U:ok", status_lines(s)[4].c_str());
}

// Kein Sensor ok -> alle vier Kürzel zeigen "--".
void test_sensors_all_missing() {
    auto s = base();
    TEST_ASSERT_EQUAL_STRING("B:-- M:-- D:-- U:--", status_lines(s)[4].c_str());
}

// Gemischt: nur BMP280 ok -> die anderen drei bleiben unabhängig "--".
void test_sensors_mixed() {
    auto s = base();
    s.bmp_ok = true;  // mpu_ok, ds18b20_ok, uv_ok bleiben false
    TEST_ASSERT_EQUAL_STRING("B:ok M:-- D:-- U:--", status_lines(s)[4].c_str());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_layout_has_four_lines_in_order);
    RUN_TEST(test_gps_silent);
    RUN_TEST(test_gps_waiting);
    RUN_TEST(test_gps_fix_shows_sat_count);
    RUN_TEST(test_sd_ok_and_missing);
    RUN_TEST(test_phase_uses_to_string);
    RUN_TEST(test_sensors_all_ok);
    RUN_TEST(test_sensors_all_missing);
    RUN_TEST(test_sensors_mixed);
    return UNITY_END();
}
