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

    const bool low = altitude_m < cfg_.ground_altitude_m;

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
            // Schneller Höhenverlust = Burst → Sinkflug. Erst nach
            // min_descent_ms anhaltendem Sinken (entprellt einzelne GPS-
            // Höhensprünge im turbulenten Aufstieg). Kein Rückweg.
            if (vspeed_ <= cfg_.descent_rate_mps) {
                if (descending_since_ == 0) descending_since_ = t_ms;
                if (t_ms - descending_since_ >= cfg_.min_descent_ms) {
                    phase_ = Phase::Descent;
                }
            } else {
                descending_since_ = 0;
            }
            break;

        case Phase::Descent:
            // Landung: wieder niedrig UND die Höhe bleibt landed_hold_ms lang
            // in einem ±landed_band_m-Band. Bewusst über die POSITION statt der
            // Momentangeschwindigkeit: vspeed = (verrauschte Höhendifferenz)/Δt
            // verstärkt GPS-Rauschen und würde am liegenden Ballon dauernd über
            // die Schwelle zappeln → Landung würde nie erkannt. Ein einzelner
            // Höhen-Ausreißer > Band verschiebt nur den Anker und startet das
            // Fenster neu (kein Landed-Fehlalarm), blockiert die Erkennung aber
            // nicht dauerhaft, solange die Höhe im Mittel ruhig bleibt.
            if (low && band_since_ != 0 &&
                altitude_m >= band_ref_m_ - cfg_.landed_band_m &&
                altitude_m <= band_ref_m_ + cfg_.landed_band_m) {
                if (t_ms - band_since_ >= cfg_.landed_hold_ms) {
                    phase_ = Phase::Landed;
                }
            } else if (low) {
                // Neues Band um die aktuelle Höhe verankern, Fenster starten.
                band_ref_m_ = altitude_m;
                band_since_ = t_ms;
            } else {
                // Noch zu hoch → gar kein Landekandidat.
                band_since_ = 0;
            }
            break;

        case Phase::Landed:
            // Endzustand: Bergungsmodus. Bleibt Landed.
            break;
    }

    return phase_;
}

Phase FlightPhaseDetector::update_no_altitude(uint32_t t_ms) {
    // Update ohne neue Höhenmessung. Hilfreich wenn nur eine Quelle ausfällt
    // (z.B. GPS kein Fix, aber Barometer läuft noch). Gibt einfach die zuletzt
    // bekannte Phase zurück, ohne den Detektor zu ändern.
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
