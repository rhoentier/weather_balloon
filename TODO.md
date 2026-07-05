# ToDo — Wetterballon Microcontroller-Projekt

Übersicht der Projektschritte. Reihenfolge von oben nach unten.

---

## ✅ Schritt 1 — Technisches Konzept (abgeschlossen)

- [x] Anforderungen geklärt (Logging + Live-Tracking, < 200 g, < 100 €, 2–3 h Flug)
- [x] Hardware recherchiert (Kern: Heltec WiFi LoRa 32 V2 [ESP32+LoRa] + NEO-6M statt T-Beam, Sensoren)
- [x] Architektur, Sensorik, Stromversorgung, Software-Konzept festgelegt
- [x] Bodenstation ausgearbeitet
- [x] Konkrete Bezugsquellen recherchiert (Heltec V2 + NEO-6M bei Funduino/BerryBase)
- [x] Konzept-Dokument geschrieben
  → `docs/superpowers/specs/2026-06-25-microcontroller-konzept-design.md`

---

## ⬜ Schritt 2 — Implementierungsplan erstellen

Detaillierter Umsetzungsplan auf Basis des Konzepts. Inhalt:

- [x] **Pinbelegung / Verdrahtung** am Heltec V2 (freie GPIOs aus Pinout; UART für GPS, SPI für SD, I²C, 1-Wire, ADC1 für UV — LoRa & OLED sind board-intern belegt)
  → `docs/superpowers/specs/2026-06-27-pinbelegung-heltec-v2.md`
- [ ] **Software-Module** definieren:
  - [x] GPS-Init inkl. Flight-Mode 6 (kritisch!) — Logik (`lib/telemetry/ubx`) nativ
        getestet, Sende-Code (`src/flight/gps_flightmode`) geschrieben; **Board-Verifikation offen**
  - [ ] Sensor-Lesung + Auto-Detect (I²C-Scan)
  - [ ] Datensatz-Bau + CSV-Logging auf microSD — CSV-Format (`lib/telemetry/record`)
        nativ getestet; SD-Schreiben (`src/flight`) noch offen
  - [ ] LoRa-Telemetrie (Sendeformat + Parameter)
  - [x] Flugphasen-Erkennung (PreFlight / Ascent / Descent / Landed) — `lib/telemetry/flight_phase`, nativ getestet
  - [ ] Bergungsmodus (Spar-Logging + GPS-Bake)
  - [ ] Watchdog / Robustheit
- [ ] **Bodenstations-Software** (Empfänger-Firmware + Laptop-Anzeige)
- [ ] **Testreihenfolge** festlegen:
  - [ ] GPS-Flight-Mode am Boden verifizieren
  - [ ] LoRa-Reichweitentest
  - [ ] microSD-Modul am Heltec: 3,3 V vs. 5 V testen, Schreiben/Lesen verifizieren (FAT32-Karte)
  - [ ] **⚠️ Batteriespannung messen** (4× Lithium-AA frisch + unter Last + kalt) → muss am 5V-Pin
        **zwischen 4,7 V und 6,0 V** bleiben (Heltec-Datenblatt). Frische L91 liefern ggf. >6 V →
        ggf. Step-Down auf 5,0 V nötig. Vor Integrationstest klären!
  - [ ] Integrationstest (alle Sensoren + Logging + Funk)
  - [ ] Kältetest (Gefrierfach) & Stromlaufzeit-Test
  - [ ] OLED-Boden-Check am Board: Zeilen plausibel; beim Übergang PreFlight→Ascent
        geht das Display aus (Vext-Abschaltung verifizieren — OLED↔Vext-Kopplung V2)
  - [ ] Generalprobe (kompletter Durchlauf wie am Flugtag)

---

## ⬜ Schritt 3 — Hardware bestellen & aufbauen

- [x] Kern gekauft: Heltec WiFi LoRa 32 V2 + NEO-6M (GPS)
- [x] Sensoren gekauft: BMP280, MPU-6050, DS18B20, GUVA-S12SD, microSD-Modul + Karte
- [x] Strom gekauft: 4× Energizer Ultimate Lithium AA + Halter
- [x] Kleinteile gekauft: Kabel, Stiftleisten, Kondensatoren
- [x] **Action-Cam besorgen** (leicht ~50–70 g; Laufzeit + Kälte vorab prüfen)
- [ ] **Bodenstation bestellen** (2. Heltec-Board + ggf. Yagi)
- [ ] Hardware aufbauen / verdrahten (ggf. löten)

---

## ⬜ Schritt 4 — Software implementieren & testen

- [ ] Firmware Flug-Einheit (gemäß Plan aus Schritt 2)
- [ ] Firmware + Anzeige Bodenstation
- [ ] Alle Tests aus Schritt 2 durchführen

---

## ⬜ Schritt 5 — Organisatorisches (parallel möglich)

- [x] Aufstiegserlaubnis bei zuständiger Landes-Luftfahrtbehörde beantragen
      (Schule = i. d. R. gebührenfrei, < 4 kg = „leicht")
- [x] Haftpflichtversicherung klären
- [x] Startort, Wetter/Wind-Vorhersage, Flugbahn-Prognose
- [x] Bergungs-Logistik (Fahrzeug, Karten, geladene Geräte)

---

## ⬜ Schritt 6 — Flugtag & Auswertung

- [ ] Generalprobe-Checkliste am Startort
- [ ] Start & Live-Tracking
- [ ] Bergung
- [ ] Daten auswerten (SD-Log + Empfangs-Log), im Unterricht aufbereiten
