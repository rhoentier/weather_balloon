// bmp_sensor.cpp — siehe Header. Adafruit_BMP280, HW-I2C, Adresse 0x76.
#include "bmp_sensor.h"
#include <Adafruit_BMP280.h>

namespace {
constexpr uint8_t  kBmpAddress   = 0x76;      // am Board per Scan bestätigt
constexpr float    kSeaLevelHpa  = 1013.25f;  // Standardatmosphäre (YAGNI)

Adafruit_BMP280 g_bmp;
bool            g_bmp_ok = false;
}  // namespace

bool bmp_begin() {
    // chipid-Default (BMP280_CHIPID = 0x58) passt zum gemessenen Chip.
    g_bmp_ok = g_bmp.begin(kBmpAddress);
    return g_bmp_ok;
}

void bmp_read(telemetry::TelemetryRecord& rec) {
    if (!g_bmp_ok) return;

    rec.has_bmp      = true;
    rec.temp_c       = g_bmp.readTemperature();
    rec.pressure_hpa = g_bmp.readPressure() / 100.0f;   // Pa -> hPa
    rec.alt_baro_m   = g_bmp.readAltitude(kSeaLevelHpa);
}
