// uv_sensor.cpp — siehe Header. ESP32-ADC an GPIO36, mehrfach gemittelt.
#include "uv_sensor.h"
#include "pins.h"
#include <Arduino.h>

namespace {
bool g_ok = false;

// So oft lesen und mitteln (dämpft das ADC-Rauschen; pins.h: „mehrfach mitteln").
constexpr int kSamples = 16;
}  // namespace

bool uv_begin() {
    // 11 dB Dämpfung → voller Eingangsbereich (~0..3,3 V). Der GUVA-Ausgang
    // liegt darunter, passt also locker rein. GPIO36 ist input-only (ADC1),
    // muss nicht per pinMode als Eingang gesetzt werden.
    analogSetPinAttenuation(PIN_UV_ADC, ADC_11db);
    g_ok = true;
    return true;
}

void uv_read(telemetry::TelemetryRecord& rec) {
    if (!g_ok) return;

    // Rohen ADC-Zählwert (0..4095) lesen und mitteln (dämpft Rauschen, gibt
    // durch Oversampling sub-count-Auflösung). Bewusst NICHT als mV: der ESP32-
    // ADC hat am unteren Ende einen festen Offset (~140 mV bei Nulleingang), der
    // die interessanten kleinen UV-Signale verdeckt. Der rohe Zählwert zeigt bei
    // null UV ehrlich ~0. Umrechnung in UV-Index erst am Boden.
    uint32_t sum = 0;
    for (int i = 0; i < kSamples; ++i) {
        sum += analogRead(PIN_UV_ADC);
    }
    rec.has_uv = true;
    rec.uv_raw = static_cast<float>(sum) / kSamples;
}
