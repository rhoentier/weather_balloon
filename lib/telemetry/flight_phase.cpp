// flight_phase.cpp — Implementierung der Flugphasen-Erkennung (siehe Header).

#include "flight_phase.h"

namespace telemetry {

FlightPhaseDetector::FlightPhaseDetector(const PhaseConfig& cfg) : cfg_(cfg) {}

Phase FlightPhaseDetector::update(float altitude_m, uint32_t t_ms) {
    // Erster Messpunkt: nur merken, noch keine Geschwindigkeit berechenbar.
    if (!have_last_) {
        have_last_ = true;
        last_alt_ = altitude_m;
        last_t_ = t_ms;
        return phase_;
    }

    const uint32_t dt_ms = t_ms - last_t_;
    if (dt_ms == 0) {
        return phase_;  // gleicher Zeitstempel → kein Update (Division vermeiden)
    }

    vspeed_ = (altitude_m - last_alt_) / (dt_ms / 1000.0f);
    last_alt_ = altitude_m;
    last_t_ = t_ms;

    const bool low    = altitude_m < cfg_.ground_altitude_m;
    const bool stable = vspeed_ < cfg_.landed_speed_mps &&
                        vspeed_ > -cfg_.landed_speed_mps;

    switch (phase_) {
        case Phase::PreFlight:
            // Aufstieg erst nach min_ascent_ms anhaltendem Steigen (entprellt
            // GPS-Rauschen am Boden).
            if (vspeed_ >= cfg_.ascent_rate_mps) {
                if (ascending_since_ == 0) ascending_since_ = t_ms;
                if (t_ms - ascending_since_ >= cfg_.min_ascent_ms) {
                    phase_ = Phase::Ascent;
                }
            } else {
                ascending_since_ = 0;
            }
            break;

        case Phase::Ascent:
            // Schneller Höhenverlust = Burst → Sinkflug. (Kein Rückweg.)
            if (vspeed_ <= cfg_.descent_rate_mps) {
                phase_ = Phase::Descent;
            }
            break;

        case Phase::Descent:
            // Landung: wieder niedrig UND über landed_hold_ms hinweg stabil.
            if (low && stable) {
                if (stable_since_ == 0) stable_since_ = t_ms;
                if (t_ms - stable_since_ >= cfg_.landed_hold_ms) {
                    phase_ = Phase::Landed;
                }
            } else {
                stable_since_ = 0;
            }
            break;

        case Phase::Landed:
            // Endzustand: Bergungsmodus. Bleibt Landed.
            break;
    }

    return phase_;
}

const char* to_string(Phase p) {
    switch (p) {
        case Phase::PreFlight: return "PREFLIGHT";
        case Phase::Ascent:    return "ASCENT";
        case Phase::Descent:   return "DESCENT";
        case Phase::Landed:    return "LANDED";
    }
    return "UNKNOWN";
}

} // namespace telemetry
