// bmp_sensor.cpp — siehe Header. Adafruit_BMP280, HW-I2C, Adresse 0x76.
#include "bmp_sensor.h"
#include <Arduino.h>
#include <Adafruit_BMP280.h>

namespace {
constexpr uint8_t kBmpAddress   = 0x76;      // am Board per Scan bestätigt
constexpr float   kStdAtmoHpa   = 1013.25f;  // Standardatmosphäre (Fallback)

Adafruit_BMP280 g_bmp;
bool            g_bmp_ok  = false;
// Referenzdruck für die Höhe. Wird beim Start auf den gemessenen Startort-
// Druck gesetzt → alt_baro_m ist dann die Höhe ÜBER DEM START (Start ≈ 0 m).
// Bleibt auf der Standardatmosphäre, falls der Sensor beim Start fehlt oder
// einen unplausiblen Wert liefert.
float           g_ref_hpa = kStdAtmoHpa;

// Plausibilitätsfenster für einen Boden-Luftdruck (grob Meereshöhe..Hochgebirge).
bool plausible_hpa(float p) { return p > 300.0f && p < 1100.0f; }
}  // namespace

bool bmp_begin() {
    // chipid-Default (BMP280_CHIPID = 0x58) passt zum gemessenen Chip.
    g_bmp_ok = g_bmp.begin(kBmpAddress);
    if (!g_bmp_ok) return false;   // Sensor nicht da → Referenz bleibt Standard

    // Startdruck als Referenz messen: über mehrere Messungen mitteln (dämpft
    // das Sensor-Rauschen von ~±0,1 hPa). Nur übernehmen, wenn plausibel.
    float sum = 0.0f;
    int   n   = 0;
    for (int i = 0; i < 16; ++i) {
        float p = g_bmp.readPressure() / 100.0f;   // Pa -> hPa
        if (plausible_hpa(p)) { sum += p; ++n; }
        delay(10);
    }
    if (n > 0) {
        float avg = sum / n;
        if (plausible_hpa(avg)) g_ref_hpa = avg;   // sonst: Standard behalten
    }
    return true;
}

float bmp_reference_hpa() {
    return g_ref_hpa;
}

void bmp_read(telemetry::TelemetryRecord& rec) {
    if (!g_bmp_ok) return;

    rec.has_bmp      = true;
    rec.temp_c       = g_bmp.readTemperature();
    rec.pressure_hpa = g_bmp.readPressure() / 100.0f;   // Pa -> hPa
    rec.alt_baro_m   = g_bmp.readAltitude(g_ref_hpa);   // Höhe über dem Start
}
