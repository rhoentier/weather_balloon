// line_assembler.h — UART-Bytestrom -> vollständige NMEA-Zeilen.
//
// Hardware-frei (kein <Arduino.h>) -> nativ testbar. Nimmt den GPS-UART-Strom
// Byte für Byte entgegen und liefert komplette Zeilen (ohne \r und \n).
// src/flight speist Serial2.read() ein; der Zeilentakt kommt aus '\n'.

#ifndef TELEMETRY_LINE_ASSEMBLER_H
#define TELEMETRY_LINE_ASSEMBLER_H

#include <cstddef>
#include <string>

namespace telemetry {

class LineAssembler {
public:
    explicit LineAssembler(std::size_t max_len = 120);

    // Ein Byte einspeisen. Rückgabe true = eine vollständige, nicht-leere Zeile
    // ist fertig und steht in out_line (ohne \r und \n). Sonst false.
    bool push(char c, std::string& out_line);

    // Puffer verwerfen (z.B. nach einem Fehler).
    void reset();

private:
    std::string buf_;
    std::size_t max_len_;
    bool overflow_ = false;  // true = aktuelle Zeile war zu lang, bis \n verwerfen
};

} // namespace telemetry

#endif // TELEMETRY_LINE_ASSEMBLER_H
