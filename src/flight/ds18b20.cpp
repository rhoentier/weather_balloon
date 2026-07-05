// ds18b20.cpp — siehe Header. DallasTemperature auf OneWire, GPIO22, async.
#include "ds18b20.h"
#include "pins.h"
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

namespace {
OneWire           g_wire(PIN_ONEWIRE);
DallasTemperature g_ds(&g_wire);
bool              g_ok      = false;
DeviceAddress     g_addr;                 // erster gefundener Sensor

// 12-Bit-Wandlung dauert lt. Datenblatt bis zu 750 ms. Etwas Reserve drauf.
constexpr uint32_t kConversionMs = 800;
uint32_t          g_started_ms = 0;       // Start der laufenden Wandlung
}  // namespace

bool ds_begin() {
    g_ds.begin();
    if (g_ds.getDeviceCount() == 0)   return false;
    if (!g_ds.getAddress(g_addr, 0))  return false;  // ersten Sensor adressieren

    g_ds.setResolution(g_addr, 12);
    // Wandlung selbst pollen statt requestTemperatures() blockieren zu lassen.
    g_ds.setWaitForConversion(false);
    g_ok = true;

    // Erste Wandlung sofort anstoßen, damit ds_update() bald einen Wert hat.
    g_ds.requestTemperaturesByAddress(g_addr);
    g_started_ms = millis();
    return true;
}

void ds_update(telemetry::TelemetryRecord& rec) {
    if (!g_ok) return;

    // Wandlung noch nicht fertig -> nichts tun (nicht blockieren).
    if ((millis() - g_started_ms) < kConversionMs) return;

    float t = g_ds.getTempC(g_addr);
    if (t != DEVICE_DISCONNECTED_C) {
        rec.has_ds     = true;
        rec.temp_ext_c = t;
    }
    // Nächste Wandlung anstoßen (unabhängig vom Leseerfolg weiterlaufen).
    g_ds.requestTemperaturesByAddress(g_addr);
    g_started_ms = millis();
}
