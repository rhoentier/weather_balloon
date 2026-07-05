// test_line_assembler.cpp — native Tests für den Zeilen-Assembler.
//   pio test -e native

#include <unity.h>
#include "line_assembler.h"
#include <string>

using namespace telemetry;

void setUp() {}
void tearDown() {}

// Hilfsroutine: speist einen ganzen String Byte für Byte ein und sammelt
// alle fertigen Zeilen in einem Vektor-Ersatz (hier: durch \x01 getrennt).
// Rückgabe: Anzahl fertiger Zeilen; die Zeilen landen in 'lines'.
static int feed(LineAssembler& a, const std::string& in, std::string lines[], int max_lines) {
    int n = 0;
    for (char c : in) {
        std::string out;
        if (a.push(c, out) && n < max_lines) lines[n++] = out;
    }
    return n;
}

// Einfache Zeile mit \n -> genau eine fertige Zeile ohne Zeilenende.
void test_simple_line() {
    LineAssembler a;
    std::string lines[4];
    int n = feed(a, "ABC\n", lines, 4);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_STRING("ABC", lines[0].c_str());
}

// Fragmentiert eingespeist -> gleiches Ergebnis wie am Stück.
void test_fragmented() {
    LineAssembler a;
    std::string out;
    TEST_ASSERT_FALSE(a.push('A', out));
    TEST_ASSERT_FALSE(a.push('B', out));
    TEST_ASSERT_TRUE(a.push('\n', out));
    TEST_ASSERT_EQUAL_STRING("AB", out.c_str());
}

// CRLF: \r wird verworfen, Inhalt bleibt sauber.
void test_crlf_stripped() {
    LineAssembler a;
    std::string lines[4];
    int n = feed(a, "ABC\r\n", lines, 4);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_STRING("ABC", lines[0].c_str());
}

// Zwei Zeilen am Stück -> zwei fertige Zeilen in Reihenfolge.
void test_multiple_lines() {
    LineAssembler a;
    std::string lines[4];
    int n = feed(a, "A\nB\n", lines, 4);
    TEST_ASSERT_EQUAL_INT(2, n);
    TEST_ASSERT_EQUAL_STRING("A", lines[0].c_str());
    TEST_ASSERT_EQUAL_STRING("B", lines[1].c_str());
}

// Leere Zeile (\n ohne Inhalt) -> nichts geliefert.
void test_empty_line_ignored() {
    LineAssembler a;
    std::string out;
    TEST_ASSERT_FALSE(a.push('\n', out));
}

// Overflow: Zeile länger als max_len ohne \n wird komplett verworfen;
// die darauffolgende gültige Zeile kommt sauber durch.
void test_overflow_then_recover() {
    LineAssembler a(4);              // kleiner Puffer für den Test
    std::string lines[4];
    // "ABCDEFG" (7 > 4) -> Overflow; \n schließt die defekte Zeile ab (kein Liefern);
    // dann "OK\n" muss sauber kommen.
    int n = feed(a, "ABCDEFG\nOK\n", lines, 4);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_STRING("OK", lines[0].c_str());
}

// Grenzfall: Zeile mit exakt max_len Zeichen + \n wird noch geliefert
// (Overflow greift nur bei mehr als max_len Zeichen OHNE \n).
void test_exact_max_len_delivered() {
    LineAssembler a(4);
    std::string lines[4];
    int n = feed(a, "ABCD\n", lines, 4);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_STRING("ABCD", lines[0].c_str());
}

// reset() leert einen halb gefüllten Puffer.
void test_reset_clears_partial() {
    LineAssembler a;
    std::string out;
    a.push('X', out);                // halb gefüllt
    a.reset();
    // Nach reset beginnt eine frische Zeile: "Y\n" -> "Y", nicht "XY".
    TEST_ASSERT_FALSE(a.push('Y', out));
    TEST_ASSERT_TRUE(a.push('\n', out));
    TEST_ASSERT_EQUAL_STRING("Y", out.c_str());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_simple_line);
    RUN_TEST(test_fragmented);
    RUN_TEST(test_crlf_stripped);
    RUN_TEST(test_multiple_lines);
    RUN_TEST(test_empty_line_ignored);
    RUN_TEST(test_overflow_then_recover);
    RUN_TEST(test_exact_max_len_delivered);
    RUN_TEST(test_reset_clears_partial);
    return UNITY_END();
}
