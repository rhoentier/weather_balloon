// ubx.h — u-blox UBX-Protokoll: GPS-Flight-Mode (Konzept §6, Baustein 1)
//
// KRITISCH: Der NEO-6M muss in den dynamischen Modell 6 ("Airborne <1g")
// geschaltet werden, sonst schaltet er oberhalb ~18 km ab (CoCom). Das ist
// der häufigste Fehler bei Höhenballons.
//
// BEWUSST hardware-frei: Diese Datei baut nur die Byte-Nachricht und prüft die
// Antwort. Das Senden über UART macht die Flug-Firmware (src/flight). So ist
// die fehleranfällige Byte-/Checksummen-Logik nativ auf dem Mac testbar —
// gegen die bekannt-korrekte UKHAS-Referenzsequenz.

#ifndef TELEMETRY_UBX_H
#define TELEMETRY_UBX_H

#include <cstddef>
#include <cstdint>

namespace telemetry {

// UBX-CFG-NAV5 für Dynamic Model 6 ist 44 Bytes:
//   2 Header + 2 Class/ID + 2 Length + 36 Payload + 2 Checksum
constexpr size_t UBX_CFG_NAV5_LEN = 44;

// Berechnet die UBX-Prüfsumme (8-Bit Fletcher) über [data, data+len).
// Bei UBX läuft sie über Class+ID+Length+Payload (NICHT über die zwei
// Sync-Bytes 0xB5 0x62). Schreibt Ergebnis nach ck_a / ck_b.
void ubx_checksum(const uint8_t* data, size_t len, uint8_t& ck_a, uint8_t& ck_b);

// Schreibt die komplette UBX-CFG-NAV5-Nachricht (Dynamic Model 6, Airborne
// <1g) in out. out muss mindestens UBX_CFG_NAV5_LEN groß sein. Inkl. korrekt
// berechneter Prüfsumme am Ende.
void build_cfg_nav5_airborne(uint8_t* out);

// Prüft, ob buf[0..len) ein gültiges UBX-ACK-ACK (Class 0x05, ID 0x01) für die
// CFG-NAV5-Nachricht (Class 0x06, ID 0x24) ist. true = GPS hat bestätigt.
bool is_cfg_nav5_ack(const uint8_t* buf, size_t len);

} // namespace telemetry

#endif // TELEMETRY_UBX_H
