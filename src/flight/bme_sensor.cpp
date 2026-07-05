// bme_sensor.cpp — siehe Header. Adafruit_BME280, HW-I2C, Adresse 0x76.
#include "bme_sensor.h"
#include <Adafruit_BME280.h>

namespace {
constexpr uint8_t  kBmeAddress   = 0x76;
constexpr float    kSeaLevelHpa  = 1013.25f;  // Standardatmosphäre (YAGNI)

Adafruit_BME280 g_bme;
bool             g_bme_ok = false;
}  // namespace

bool bme_begin() {
    g_bme_ok = g_bme.begin(kBmeAddress);
    return g_bme_ok;
}

void bme_read(telemetry::TelemetryRecord& rec) {
    if (!g_bme_ok) return;

    rec.has_bme      = true;
    rec.temp_c       = g_bme.readTemperature();
    rec.pressure_hpa = g_bme.readPressure() / 100.0f;   // Pa -> hPa
    rec.alt_baro_m   = g_bme.readAltitude(kSeaLevelHpa);
    rec.humidity_pct = g_bme.readHumidity();
}
