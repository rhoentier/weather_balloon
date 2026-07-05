// ds18b20.h — DS18B20 Außentemperatur (1-Wire) via DallasTemperature-Lib.
//
// Board-Hardware-Teil (kein nativer Test, analog zu bmp_sensor.cpp) — wird am
// Heltec V2 verifiziert. Pin: GPIO22 (1-Wire, 4,7k Pull-up nach 3V3).
//
// Warum ein eigener Temperatursensor neben dem BMP280? Der BMP280 sitzt IM
// Modul und misst dessen Eigenwärme mit. Der DS18B20 hängt am Kabel außen und
// liefert die echte Umgebungs-/Außentemperatur.
//
// NON-BLOCKING: Eine DS18B20-Wandlung dauert bei 12 Bit bis zu ~750 ms. Würde
// bmp_read()-artig blockierend gemessen, stünde solange der GPS-UART-Loop. Daher
// asynchron: ds_update() stößt eine Wandlung an, holt beim nächsten Aufruf den
// fertigen Wert ab und startet die nächste — nie blockierend.
#ifndef FLIGHT_DS18B20_H
#define FLIGHT_DS18B20_H

#include "record.h"

// 1-Wire-Bus starten und ersten Sensor suchen. Setzt non-blocking-Modus.
// Rückgabe: true, wenn mindestens ein DS18B20 am Bus antwortet.
bool ds_begin();

// Non-blocking: Ist ein Messwert fertig, wird er in rec (has_ds/temp_ext_c)
// geschrieben und die nächste Wandlung angestoßen. Sonst bleibt rec unverändert.
// Ohne erfolgreiches ds_begin() ein No-op.
void ds_update(telemetry::TelemetryRecord& rec);

#endif // FLIGHT_DS18B20_H
