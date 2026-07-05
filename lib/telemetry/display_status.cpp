// display_status.cpp — siehe Header.
#include "display_status.h"

namespace telemetry {

std::vector<std::string> status_lines(const DisplayState& s) {
    std::string gps;
    switch (s.gps) {
        case GpsDisp::Silent:  gps = "GPS: --";       break;
        case GpsDisp::Waiting: gps = "GPS: warte...";  break;
        case GpsDisp::Fix:     gps = "GPS: Fix " + std::to_string((int)s.sats) + " Sat"; break;
    }
    std::string sensors = std::string("B:") + (s.bme_ok     ? "ok" : "--")
                         + " M:"             + (s.mpu_ok     ? "ok" : "--")
                         + " D:"             + (s.ds18b20_ok ? "ok" : "--")
                         + " U:"             + (s.uv_ok      ? "ok" : "--");
    return {
        "Wetterballon",
        gps,
        s.sd_ok ? "SD: ok" : "SD: --",
        std::string("Phase: ") + to_string(s.phase),
        sensors,
    };
}

} // namespace telemetry
