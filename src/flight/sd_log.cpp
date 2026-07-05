// sd_log.cpp — siehe sd_log.h. Hardware-Teil, am Board verifizieren.

#include "sd_log.h"
#include <SD.h>
#include <SPI.h>
#include "pins.h"
#include "record.h"   // telemetry::csv_header()

static bool s_ready = false;
static const char* kLogPath = "/flight.csv";

bool sd_log_begin() {
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
