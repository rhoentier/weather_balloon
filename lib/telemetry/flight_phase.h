// flight_phase.h — Flugphasen-Erkennung (Konzept §6)
//
// BEWUSST hardware-frei: kein <Arduino.h>, keine Sensoren, keine globalen
// Zustände. Nur reine Logik → läuft 1:1 als nativer Unit-Test auf dem Mac.
// Die Flug-Firmware füttert diese Logik mit Höhe + Zeit; die Logik entscheidet
// die Phase. So ist das sicherheitskritischste Verhalten (Landung erkennen!)
// schnell und wiederholbar testbar, ohne den Ballon fliegen zu lassen.

#ifndef TELEMETRY_FLIGHT_PHASE_H
#define TELEMETRY_FLIGHT_PHASE_H

#include <cstdint>

namespace telemetry {

enum class Phase {
    PreFlight,  // am Boden, Höhe niedrig & stabil  → Funktest, schnelles Logging
    Ascent,     // Höhe steigt                       → volles Logging + Telemetrie
    Descent,    // Höhe sinkt schnell (nach Burst)   → wie Ascent
    Landed      // wieder niedrig & stabil           → Bergungsmodus (Spar-Logging + Bake)
};

// Parameter der Erkennung. Defaults sind konservativ; im Kältetest /
// in der Generalprobe verifizieren und ggf. anpassen.
struct PhaseConfig {
    float ascent_rate_mps   = 1.0f;   // ab dieser Steigrate gilt es als Aufstieg
    float descent_rate_mps  = -3.0f;  // ab dieser Sinkrate gilt es als Sinkflug
    float ground_altitude_m = 800.0f; // unterhalb -> Kandidat für "am Boden"
    float landed_speed_mps  = 0.5f;   // |Vertikalgeschw.| darunter = stabil
    uint32_t landed_hold_ms = 60000;  // so lange stabil+niedrig -> Landed
    uint32_t min_ascent_ms  = 5000;   // Mindestzeit steigend, bevor Ascent (Entprellen)
};

// Zustandsautomat. Bekommt Messpunkte (Höhe + Zeitstempel) und liefert die
// aktuelle Phase. Einmal in Descent/Landed wird NICHT zurück zu Ascent
// gewechselt (kein Zurückspringen durch GPS-Rauschen).
class FlightPhaseDetector {
public:
    explicit FlightPhaseDetector(const PhaseConfig& cfg = PhaseConfig{});

    // Einen neuen Messpunkt verarbeiten. altitude_m = Höhe (GPS oder baro),
    // t_ms = monotone Zeit in Millisekunden. Gibt die aktuelle Phase zurück.
    Phase update(float altitude_m, uint32_t t_ms);

    Phase phase() const { return phase_; }

    // Vertikalgeschwindigkeit des letzten Updates (m/s), für Telemetrie/Debug.
    float vertical_speed_mps() const { return vspeed_; }

private:
    PhaseConfig cfg_;
    Phase phase_ = Phase::PreFlight;
    bool  have_last_ = false;
    float last_alt_ = 0.0f;
    uint32_t last_t_ = 0;
    float vspeed_ = 0.0f;
    uint32_t ascending_since_ = 0;  // seit wann steigend (Entprellen)
    uint32_t stable_since_ = 0;     // seit wann niedrig+stabil (Landed-Halten)
};

// Phase als kurzer Text (für CSV / Display / Telemetrie).
const char* to_string(Phase p);

} // namespace telemetry

#endif // TELEMETRY_FLIGHT_PHASE_H
