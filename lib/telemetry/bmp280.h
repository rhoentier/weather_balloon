// bmp280.h — BMP280-Umrechnung: Rohwerte + Kalibrierung -> physikalische Größen.
//
// Hardware-frei (kein <Arduino.h>) -> nativ testbar. Kennt NUR den BMP280,
// nicht das CSV-Format und KEIN I²C. Der src/flight-Aufrufer liest per I²C die
// Kalibrierkonstanten (einmalig) und die Rohwerte (pro Zyklus) und ruft diese
// Funktionen. Formeln exakt nach BST-BMP280-DS001 Rev 1.26, 3.11.3 (32-bit).

#ifndef TELEMETRY_BMP280_H
#define TELEMETRY_BMP280_H

#include <cstdint>

namespace telemetry {

// Kalibrierkonstanten (Register 0x88..0x9F). Typen exakt nach Datenblatt
// (3.11.2) — Vorzeichen entscheidend, sonst grob falsche Werte!
struct Bmp280Calib {
    uint16_t dig_T1;
    int16_t  dig_T2, dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2, dig_P3, dig_P4, dig_P5,
             dig_P6, dig_P7, dig_P8, dig_P9;
};

// adc_T = 20-bit-Rohwert (positiv, in 32-bit-signed). Rückgabe in 0,01 °C
// (2508 = 25,08 °C). Setzt t_fine (Zwischengröße für die Druckformel).
int32_t bmp280_compensate_temperature(int32_t adc_T, const Bmp280Calib& c,
                                       int32_t& t_fine);

// adc_P = 20-bit-Rohwert. t_fine MUSS vorher von compensate_temperature stammen.
// Rückgabe in Pa direkt (100656 = 1006,56 hPa).
uint32_t bmp280_compensate_pressure(int32_t adc_P, const Bmp280Calib& c,
                                     int32_t t_fine);

// Barometrische Höhe (m) aus Druck (Pa) + Referenzdruck QNH (Pa).
float bmp280_altitude_m(float pressure_pa, float sea_level_pa);

} // namespace telemetry

#endif // TELEMETRY_BMP280_H
