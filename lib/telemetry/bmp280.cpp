// bmp280.cpp — BMP280-Umrechnung (siehe bmp280.h). Hardware-frei.
// Integer-Kompensation exakt nach BST-BMP280-DS001 Rev 1.26, 3.11.3.

#include "bmp280.h"
#include <cmath>

namespace telemetry {

int32_t bmp280_compensate_temperature(int32_t adc_T, const Bmp280Calib& c,
                                       int32_t& t_fine) {
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)c.dig_T1 << 1))) *
                    ((int32_t)c.dig_T2)) >> 11;
    int32_t var2 = (((((adc_T >> 4) - ((int32_t)c.dig_T1)) *
                      ((adc_T >> 4) - ((int32_t)c.dig_T1))) >> 12) *
                    ((int32_t)c.dig_T3)) >> 14;
    t_fine = var1 + var2;
    return (t_fine * 5 + 128) >> 8;
}

uint32_t bmp280_compensate_pressure(int32_t adc_P, const Bmp280Calib& c,
                                     int32_t t_fine) {
    int32_t var1 = (((int32_t)t_fine) >> 1) - (int32_t)64000;
    int32_t var2 = (((var1 >> 2) * (var1 >> 2)) >> 11) * ((int32_t)c.dig_P6);
    var2 = var2 + ((var1 * ((int32_t)c.dig_P5)) << 1);
    var2 = (var2 >> 2) + (((int32_t)c.dig_P4) << 16);
    var1 = (((c.dig_P3 * (((var1 >> 2) * (var1 >> 2)) >> 13)) >> 3) +
            ((((int32_t)c.dig_P2) * var1) >> 1)) >> 18;
    var1 = ((((32768 + var1)) * ((int32_t)c.dig_P1)) >> 15);
    if (var1 == 0) {
        return 0;  // Division durch Null vermeiden
    }
    uint32_t p = (((uint32_t)(((int32_t)1048576) - adc_P) - (var2 >> 12))) * 3125;
    if (p < 0x80000000) {
        p = (p << 1) / ((uint32_t)var1);
    } else {
        p = (p / (uint32_t)var1) * 2;
    }
    var1 = (((int32_t)c.dig_P9) * ((int32_t)(((p >> 3) * (p >> 3)) >> 13))) >> 12;
    var2 = (((int32_t)(p >> 2)) * ((int32_t)c.dig_P8)) >> 13;
    p = (uint32_t)((int32_t)p + ((var1 + var2 + c.dig_P7) >> 4));
    return p;
}

float bmp280_altitude_m(float pressure_pa, float sea_level_pa) {
    // Internationale barometrische Höhenformel (Standardatmosphäre).
    return 44330.0f * (1.0f - std::pow(pressure_pa / sea_level_pa,
                                       1.0f / 5.255f));
}

} // namespace telemetry
