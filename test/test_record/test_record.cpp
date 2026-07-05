// test_record.cpp — native Tests für das Telemetrie-/CSV-Format.
//   pio test -e native
//
// Kern-Anforderungen:
//   - EIN Format für SD-Log und LoRa-Telemetrie
//   - direkt in Excel/Python lesbar (CSV, Komma-getrennt)
//   - fehlende Werte = leeres Feld
//   - Round-Trip: csv_row -> parse_csv_row liefert denselben Record
//     (so vergleichbar wie SD-Log vs. Empfangs-Log, Konzept §7.2)

#include <unity.h>
#include "record.h"
#include <string>

using namespace telemetry;

void setUp() {}
void tearDown() {}

// Header und Datenzeile müssen gleich viele Spalten haben.
void test_header_and_row_same_column_count() {
    TelemetryRecord r;
    std::string header = csv_header();
    std::string row = csv_row(r);

    auto count_cols = [](const std::string& s) {
        int n = 1;
        for (char c : s) if (c == ',') ++n;
        return n;
    };
    TEST_ASSERT_EQUAL_INT(count_cols(header), count_cols(row));
}

// Header beginnt mit den Pflichtspalten in definierter Reihenfolge.
void test_header_starts_with_time_and_phase() {
    std::string h = csv_header();
    TEST_ASSERT_EQUAL_INT(0, h.rfind("t_ms,utc,phase,", 0));  // beginnt mit ...
}

// Ohne GPS-Fix bleiben die GPS-Spalten LEER (kein 0.0 vortäuschen).
void test_row_without_fix_has_empty_gps_fields() {
    TelemetryRecord r;
    r.t_ms = 1234;
    r.phase = Phase::PreFlight;
    r.has_fix = false;

    std::string row = csv_row(r);
    // beginnt mit Zeit + (leeres utc) + Phase
    TEST_ASSERT_EQUAL_INT(0, row.rfind("1234,,PREFLIGHT,", 0));
    // und enthält die vier leeren GPS-Felder am Ende: ",,,,"
    TEST_ASSERT_TRUE(row.find(",,,,") != std::string::npos);
}

// Mit Fix erscheinen die GPS-Werte.
void test_row_with_fix_contains_values() {
    TelemetryRecord r;
    r.t_ms = 5000;
    r.phase = Phase::Ascent;
    r.has_fix = true;
    r.lat = 48.137000;
    r.lon = 11.575000;
    r.alt_gps_m = 1234.5f;
    r.sats = 9;

    std::string row = csv_row(r);
    TEST_ASSERT_TRUE(row.find("ASCENT") != std::string::npos);
    TEST_ASSERT_TRUE(row.find("48.137") != std::string::npos);
    TEST_ASSERT_TRUE(row.find("11.575") != std::string::npos);
}

// Round-Trip MIT Fix: schreiben -> parsen -> gleiche Werte.
void test_roundtrip_with_fix() {
    TelemetryRecord in;
    in.t_ms = 42000;
    in.phase = Phase::Descent;
    in.has_fix = true;
    in.lat = 48.1372345;
    in.lon = 11.5754321;
    in.alt_gps_m = 8765.0f;
    in.sats = 11;

    TelemetryRecord out;
    TEST_ASSERT_TRUE(parse_csv_row(csv_row(in), out));

    TEST_ASSERT_EQUAL_UINT32(in.t_ms, out.t_ms);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(in.phase), static_cast<int>(out.phase));
    TEST_ASSERT_TRUE(out.has_fix);
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, in.lat, out.lat);
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, in.lon, out.lon);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, in.alt_gps_m, out.alt_gps_m);
    TEST_ASSERT_EQUAL_UINT8(in.sats, out.sats);
}

// Round-Trip OHNE Fix: leere Felder müssen has_fix=false zurückgeben.
void test_roundtrip_without_fix() {
    TelemetryRecord in;
    in.t_ms = 7000;
    in.phase = Phase::PreFlight;
    in.has_fix = false;

    TelemetryRecord out;
    out.has_fix = true;  // bewusst vorbelegen, muss überschrieben werden
    TEST_ASSERT_TRUE(parse_csv_row(csv_row(in), out));

    TEST_ASSERT_EQUAL_UINT32(7000, out.t_ms);
    TEST_ASSERT_FALSE(out.has_fix);
}

// Eine kaputte/leere Zeile wird sauber abgelehnt.
void test_garbage_line_rejected() {
    TelemetryRecord out;
    TEST_ASSERT_FALSE(parse_csv_row("", out));
}

// Header führt utc an zweiter Stelle.
void test_header_has_utc_second() {
    std::string h = csv_header();
    TEST_ASSERT_EQUAL_INT(0, h.rfind("t_ms,utc,", 0));
}

// Row mit UTC -> hh:mm:ss mit führenden Nullen.
void test_row_with_utc_formatted() {
    TelemetryRecord r;
    r.t_ms = 100;
    r.has_utc = true;
    r.utc_h = 8; r.utc_min = 5; r.utc_s = 9;
    std::string row = csv_row(r);
    TEST_ASSERT_TRUE(row.find("08:05:09") != std::string::npos);
}

// Row ohne UTC -> leeres Feld an Position 2 (direkt nach t_ms).
void test_row_without_utc_empty() {
    TelemetryRecord r;
    r.t_ms = 100;
    r.has_utc = false;
    std::string row = csv_row(r);
    TEST_ASSERT_EQUAL_INT(0, row.rfind("100,,", 0));  // t_ms, dann leeres utc
}

// Round-Trip MIT UTC.
void test_roundtrip_with_utc() {
    TelemetryRecord in;
    in.t_ms = 55000;
    in.phase = Phase::Ascent;
    in.has_utc = true;
    in.utc_h = 23; in.utc_min = 59; in.utc_s = 1;

    TelemetryRecord out;
    TEST_ASSERT_TRUE(parse_csv_row(csv_row(in), out));
    TEST_ASSERT_TRUE(out.has_utc);
    TEST_ASSERT_EQUAL_UINT8(23, out.utc_h);
    TEST_ASSERT_EQUAL_UINT8(59, out.utc_min);
    TEST_ASSERT_EQUAL_UINT8(1,  out.utc_s);
}

// Round-Trip OHNE UTC -> has_utc false zurück.
void test_roundtrip_without_utc() {
    TelemetryRecord in;
    in.t_ms = 6000;
    in.has_utc = false;

    TelemetryRecord out;
    out.has_utc = true;  // bewusst vorbelegen, muss überschrieben werden
    TEST_ASSERT_TRUE(parse_csv_row(csv_row(in), out));
    TEST_ASSERT_FALSE(out.has_utc);
}

// Falsche Spaltenzahl (7 statt 10) -> abgelehnt.
void test_wrong_column_count_rejected() {
    TelemetryRecord out;
    // alte 7-Spalten-Zeile ohne BMP-Felder
    TEST_ASSERT_FALSE(parse_csv_row("123,,PREFLIGHT,,,,", out));
}

// Header endet mit den drei BMP280-Spalten in fester Reihenfolge.
void test_header_ends_with_bmp_columns() {
    std::string h = csv_header();
    std::string suffix = "temp_c,pressure_hpa,alt_baro_m";
    TEST_ASSERT_TRUE(h.size() >= suffix.size());
    TEST_ASSERT_EQUAL_STRING(suffix.c_str(),
                             h.substr(h.size() - suffix.size()).c_str());
}

// Ohne BMP-Sensor bleiben die drei Spalten leer (kein 0.0 vortäuschen).
void test_row_without_bmp_has_empty_fields() {
    TelemetryRecord r;
    r.t_ms = 1;
    r.has_bmp = false;
    std::string row = csv_row(r);
    // Zeile endet mit drei leeren Feldern: ",,"  (temp, pressure, alt)
    TEST_ASSERT_EQUAL_STRING(",,",
                             row.substr(row.size() - 2).c_str());
}

// Round-Trip MIT BMP-Werten.
void test_roundtrip_with_bmp() {
    TelemetryRecord in;
    in.t_ms = 3000;
    in.phase = Phase::Ascent;
    in.has_bmp = true;
    in.temp_c = 25.08f;
    in.pressure_hpa = 1006.56f;
    in.alt_baro_m = 55.85f;

    TelemetryRecord out;
    TEST_ASSERT_TRUE(parse_csv_row(csv_row(in), out));
    TEST_ASSERT_TRUE(out.has_bmp);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, in.temp_c, out.temp_c);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, in.pressure_hpa, out.pressure_hpa);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, in.alt_baro_m, out.alt_baro_m);
}

// Round-Trip OHNE BMP -> has_bmp false zurück.
void test_roundtrip_without_bmp() {
    TelemetryRecord in;
    in.t_ms = 4000;
    in.has_bmp = false;

    TelemetryRecord out;
    out.has_bmp = true;  // bewusst vorbelegen, muss überschrieben werden
    TEST_ASSERT_TRUE(parse_csv_row(csv_row(in), out));
    TEST_ASSERT_FALSE(out.has_bmp);
}

// Header führt fix_q direkt nach phase (vor lat).
void test_header_has_fix_quality_after_phase() {
    std::string h = csv_header();
    TEST_ASSERT_EQUAL_INT(0, h.rfind("t_ms,utc,phase,fix_q,lat,", 0));
}

// fix_quality wird immer geschrieben (auch 0 = kein Fix ist ein echter Wert).
void test_row_writes_fix_quality() {
    TelemetryRecord r;
    r.t_ms = 1234;
    r.phase = Phase::PreFlight;
    r.fix_quality = 2;  // z.B. DGPS
    std::string row = csv_row(r);
    // ...,PREFLIGHT,2,...
    TEST_ASSERT_TRUE(row.find("PREFLIGHT,2,") != std::string::npos);
}

// Round-Trip erhält fix_quality.
void test_roundtrip_fix_quality() {
    TelemetryRecord in;
    in.t_ms = 9000;
    in.phase = Phase::Ascent;
    in.has_fix = true;
    in.fix_quality = 2;
    in.lat = 48.1; in.lon = 11.5;

    TelemetryRecord out;
    TEST_ASSERT_TRUE(parse_csv_row(csv_row(in), out));
    TEST_ASSERT_EQUAL_UINT8(2, out.fix_quality);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_header_and_row_same_column_count);
    RUN_TEST(test_header_starts_with_time_and_phase);
    RUN_TEST(test_row_without_fix_has_empty_gps_fields);
    RUN_TEST(test_row_with_fix_contains_values);
    RUN_TEST(test_roundtrip_with_fix);
    RUN_TEST(test_roundtrip_without_fix);
    RUN_TEST(test_garbage_line_rejected);
    RUN_TEST(test_header_has_utc_second);
    RUN_TEST(test_row_with_utc_formatted);
    RUN_TEST(test_row_without_utc_empty);
    RUN_TEST(test_roundtrip_with_utc);
    RUN_TEST(test_roundtrip_without_utc);
    RUN_TEST(test_wrong_column_count_rejected);
    RUN_TEST(test_header_has_fix_quality_after_phase);
    RUN_TEST(test_row_writes_fix_quality);
    RUN_TEST(test_roundtrip_fix_quality);
    RUN_TEST(test_header_ends_with_bmp_columns);
    RUN_TEST(test_row_without_bmp_has_empty_fields);
    RUN_TEST(test_roundtrip_with_bmp);
    RUN_TEST(test_roundtrip_without_bmp);
    return UNITY_END();
}
