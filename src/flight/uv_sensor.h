// uv_sensor.h — GUVA-S12SD UV-Sensor (analog) über den ESP32-ADC.
//
// Board-Hardware-Teil (kein nativer Test, analog zu bmp_sensor.cpp/ds18b20.cpp) —
// wird am Heltec V2 verifiziert. Pin: GPIO36 (ADC1, input-only; siehe pins.h).
//
// WAS wird geloggt? Der ROHE ADC-Zählwert (0..4095) — NICHT mV und NICHT ein
// UV-Index. Zwei Gründe: (1) die Umrechnung in UV-Index/mW·cm⁻² hängt stark vom
// konkreten Breakout (OpAmp-Verstärkung) ab und ist nicht verifiziert; (2) der
// ESP32-ADC hat am unteren Ende einen festen Offset (~140 mV bei Nulleingang),
// sodass analogReadMilliVolts() null UV als irreführende „142 mV" zeigt — der
// rohe Zählwert zeigt bei null UV ehrlich ~0. Umrechnung erst am Boden
// (Python/Excel). Passt zu YAGNI + „Fakten statt raten".
//
// KEIN async nötig (anders als DS18B20): ein analogRead dauert nur µs.
#ifndef FLIGHT_UV_SENSOR_H
#define FLIGHT_UV_SENSOR_H

#include "record.h"

// ADC-Pin konfigurieren (Dämpfung für den vollen Eingangsbereich).
// Rückgabe: true. Ein reiner Analog-Pin lässt sich — anders als ein Bus-Sensor
// (I²C/1-Wire) — nicht „finden": ob wirklich ein GUVA dranhängt, ist über den
// ADC nicht feststellbar. Wir melden daher „vorhanden", sobald der Pin gesetzt
// ist; ob der Wert plausibel ist, sieht man am Messwert selbst.
bool uv_begin();

// Liest den ADC (mehrfach gemittelt, dämpft ADC-Rauschen) und füllt
// has_uv/uv_raw in rec. Ohne erfolgreiches uv_begin() ein No-op.
void uv_read(telemetry::TelemetryRecord& rec);

#endif // FLIGHT_UV_SENSOR_H
