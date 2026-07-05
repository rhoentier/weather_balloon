// test_flight_phase.cpp — native Unit-Tests (Unity) für die Flugphasen-Logik.
// Laufen auf dem Mac:  pio test -e native
//
// Hier testen wir das sicherheitskritischste Verhalten des Flugs OHNE Hardware:
// erkennt der Automat Aufstieg, Burst/Sinkflug und vor allem die LANDUNG?

#include <unity.h>
#include "flight_phase.h"

using namespace telemetry;

// Hilfsfunktion: viele Messpunkte mit konstanter Steig-/Sinkrate einspeisen.
// Gibt den Endzeitstempel zurück, damit Tests die Zeit fortführen können.
static uint32_t feed(FlightPhaseDetector& d, float& alt, uint32_t t0,
                     float rate_mps, int steps, uint32_t step_ms = 1000) {
    uint32_t t = t0;
    for (int i = 0; i < steps; ++i) {
        t += step_ms;
        alt += rate_mps * (step_ms / 1000.0f);
        d.update(alt, t);
    }
    return t;
}

void setUp() {}
void tearDown() {}

// Startzustand ist PreFlight.
void test_starts_in_preflight() {
    FlightPhaseDetector d;
    TEST_ASSERT_EQUAL(static_cast<int>(Phase::PreFlight),
                      static_cast<int>(d.phase()));
}

// Kurzes Wackeln am Boden darf NICHT als Aufstieg gelten (Entprellen).
void test_ground_jitter_stays_preflight() {
    FlightPhaseDetector d;
    float alt = 100.0f;
    uint32_t t = 0;
    // 2 s steigend, dann wieder fallend — unter min_ascent_ms (5 s).
    t = feed(d, alt, t, 2.0f, 2);
    t = feed(d, alt, t, -2.0f, 2);
    TEST_ASSERT_EQUAL(static_cast<int>(Phase::PreFlight),
                      static_cast<int>(d.phase()));
}

// Anhaltendes Steigen → Ascent.
void test_sustained_climb_enters_ascent() {
    FlightPhaseDetector d;
    float alt = 100.0f;
    feed(d, alt, 0, 5.0f, 10);  // 10 s @ +5 m/s
    TEST_ASSERT_EQUAL(static_cast<int>(Phase::Ascent),
                      static_cast<int>(d.phase()));
}

// Voller Ablauf: Aufstieg → Burst/Sinkflug → Landung.
void test_full_flight_reaches_landed() {
    FlightPhaseDetector d;
    float alt = 100.0f;
    uint32_t t = 0;

    t = feed(d, alt, t, 5.0f, 200);   // Aufstieg auf ~1100 m
    TEST_ASSERT_EQUAL(static_cast<int>(Phase::Ascent),
                      static_cast<int>(d.phase()));

    t = feed(d, alt, t, -8.0f, 60);   // schneller Sinkflug
    TEST_ASSERT_EQUAL(static_cast<int>(Phase::Descent),
                      static_cast<int>(d.phase()));

    // gelandet: niedrig + stabil über landed_hold_ms (default 60 s)
    alt = 300.0f;
    t = feed(d, alt, t, 0.0f, 70);    // 70 s stabil, niedrig
    TEST_ASSERT_EQUAL(static_cast<int>(Phase::Landed),
                      static_cast<int>(d.phase()));
}

// Landed ist Endzustand: erneutes "Steigen" (GPS-Rauschen) darf nicht zurück.
void test_landed_is_terminal() {
    FlightPhaseDetector d;
    float alt = 100.0f;
    uint32_t t = 0;
    t = feed(d, alt, t, 5.0f, 200);
    t = feed(d, alt, t, -8.0f, 60);
    alt = 300.0f;
    t = feed(d, alt, t, 0.0f, 70);
    TEST_ASSERT_EQUAL(static_cast<int>(Phase::Landed),
                      static_cast<int>(d.phase()));

    // jetzt "fälschlich" wieder Steigen — muss Landed bleiben
    feed(d, alt, t, 5.0f, 30);
    TEST_ASSERT_EQUAL(static_cast<int>(Phase::Landed),
                      static_cast<int>(d.phase()));
}

// to_string liefert die erwarteten CSV-Labels.
void test_phase_labels() {
    TEST_ASSERT_EQUAL_STRING("PREFLIGHT", to_string(Phase::PreFlight));
    TEST_ASSERT_EQUAL_STRING("ASCENT",    to_string(Phase::Ascent));
    TEST_ASSERT_EQUAL_STRING("DESCENT",   to_string(Phase::Descent));
    TEST_ASSERT_EQUAL_STRING("LANDED",    to_string(Phase::Landed));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_starts_in_preflight);
    RUN_TEST(test_ground_jitter_stays_preflight);
    RUN_TEST(test_sustained_climb_enters_ascent);
    RUN_TEST(test_full_flight_reaches_landed);
    RUN_TEST(test_landed_is_terminal);
    RUN_TEST(test_phase_labels);
    return UNITY_END();
}
