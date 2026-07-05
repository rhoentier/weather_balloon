// line_assembler.cpp — siehe line_assembler.h. Hardware-frei.

#include "line_assembler.h"

namespace telemetry {

LineAssembler::LineAssembler(std::size_t max_len) : max_len_(max_len) {}

bool LineAssembler::push(char c, std::string& out_line) {
    if (c == '\r') return false;      // CRLF: \r immer verwerfen

    if (c == '\n') {
        if (overflow_) {              // überlange Zeile: verwerfen, resync fertig
            overflow_ = false;
            buf_.clear();             // defensiv; buf_ ist im Overflow bereits leer
            return false;
        }
        if (buf_.empty()) return false;   // leere Zeile nicht durchreichen
        out_line = buf_;
        buf_.clear();
        return true;
    }

    if (overflow_) return false;      // bis zum nächsten \n weiter verwerfen

    if (buf_.size() >= max_len_) {    // Overflow: Zeile als defekt verwerfen
        overflow_ = true;
        buf_.clear();
        return false;
    }

    buf_.push_back(c);
    return false;
}

void LineAssembler::reset() {
    buf_.clear();
    overflow_ = false;
}

} // namespace telemetry
