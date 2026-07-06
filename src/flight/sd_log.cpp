// sd_log.cpp — siehe sd_log.h. Hardware-Teil, am Board verifizieren.

#include "sd_log.h"
#include <SD.h>
#include <SPI.h>
#include "pins.h"
#include "record.h"   // telemetry::csv_header()

static bool s_ready = false;
static const char* kLogPath = "/flight.csv";

bool sd_log_begin() {
    // Heltec V2 nutzt für den LoRa/SD-SPI-Bus NICHT die ESP32-VSPI-Standardpins
    // (SCK18/MISO19/MOSI23) -> Pins explizit setzen, sonst spricht SD.begin()
    // ins Leere (f_mount-Fehler (3), egal wie sauber verkabelt ist).
    SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_SD_CS);

    // Manche HW-125-Module brauchen nach Power-On kurz Zeit, bis die
    // Karte bereit ist (Anlaufzeit intern). Ohne Verzögerung schlägt
    // SD.begin() mit einem I/O-Fehler fehl, obwohl Verkabelung + Karte ok sind.
    delay(250);

    if (!SD.begin(PIN_SD_CS)) {
        Serial.println("[flight] !!! microSD nicht gefunden — Logging aus, Betrieb laeuft !!!");
        s_ready = false;
        return false;
    }
    s_ready = true;

    // Header nur schreiben, wenn Datei neu oder leer ist (kein Doppel-Header bei Append).
    //
    // Defensives Prinzip: im Zweifel KEINEN Header schreiben. Der SPI-Bus ist
    // mit dem LoRa-Modul geteilt (bekannte Stolperfalle) — ein FILE_READ-Open
    // kann transient fehlschlagen, obwohl die Datei laengst Flugdaten enthaelt.
    // Ein fehlender Header bei einer wirklich leeren Datei ist harmlos, aber
    // ein Header MITTEN in bestehenden Flugdaten korrumpiert das CSV
    // unwiderruflich. Darum: nur bei Nicht-Existenz sicher Header schreiben;
    // existiert die Datei, Header nur im sicher verifizierten Leer-Fall.
    bool need_header;
    if (!SD.exists(kLogPath)) {
        need_header = true;   // Datei existiert nicht -> neu anlegen, Header noetig
    } else {
        need_header = false;  // existiert -> im Zweifel KEIN Header
        File f = SD.open(kLogPath, FILE_READ);
        if (f) {
            // Sonderfall: Datei existiert, ist aber sicher leer (0 Bytes),
            // z.B. weil das Board vor dem ersten Write abgestuerzt ist.
            if (f.size() == 0) need_header = true;
            f.close();
        }
        // Liess sich die Datei nicht oeffnen (transienter I/O-Fehler), bleibt
        // need_header = false — kein Risiko eines Doppel-Headers eingehen.
    }
    if (need_header) {
        File f = SD.open(kLogPath, FILE_APPEND);
        if (f) {
            f.println(telemetry::csv_header().c_str());
            f.close();
        }
    }
    Serial.println("[flight] microSD bereit — Log: /flight.csv");
    return true;
}

void sd_log(const String& line) {
    if (!s_ready) return;
    File f = SD.open(kLogPath, FILE_APPEND);
    if (!f) return;
    f.println(line);
    f.close();
}
