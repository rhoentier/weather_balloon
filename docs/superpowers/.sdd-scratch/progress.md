# SDD Progress — BMP280-Umrechnung (ohne Git)
Task 1: complete (bmp280.h/.cpp Temp + test_bmp280, 2/2 grün, review clean)
  - Minor (offen, Doku): Implementer soll RED-Lauf-Output ins Report schreiben
Task 2: complete (bmp280.cpp Druck + test, 3/3 grün, review clean, Formel unabhängig verifiziert)
Task 3: complete (bmp280_altitude_m + 2 Tests, 5/5 grün, review clean, Formel unabhängig nachgefahren)
  - Low (Doku, Scratch): Task-3-Report nennt falsche Normquelle "IEC 61131-2" — Report-Prosa, nicht Code. Regel "Fakten verifizieren" auch für Reports.
Task 4: complete (record.h/.cpp: has_bmp+temp_c+pressure_hpa+alt_baro_m, CSV jetzt 11 statt geplanter 10 Spalten)
  - Grund für 11 statt 10: paralleler Agent hat währenddessen unabhängig `fix_quality` in record.h/.cpp
    ergänzt (nicht Teil dieses Plans). Der Task-4-Implementer hat korrekt an die reale (damals schon
    veränderte) Basis angepasst statt blind den Plan zu kopieren. Ist-Abgleich nach Pause bestätigt:
    Spaltenreihenfolge/Leerfeld-Semantik/Rundtrip in record.h/.cpp konsistent, volle native-Suite
    70/70 grün (inkl. test_display_status, das im instabilen Zwischenzustand fälschlich als ERRORED
    auftauchte — war ein Artefakt, kein echter Fehler).
  - Low (Doku, Scratch): task-4-report.md vom Implementer als geschrieben gemeldet, existiert aber
    nicht auf der Platte. Code + Tests wurden stattdessen direkt von mir nachgeprüft (grep/Read/pio
    test), Review gilt auf dieser Basis als bestanden.
Task 5: complete (docs/software-flow.md aktualisiert: bmp280 im Architektur-Diagramm ✅, READ_SENS im
  loop()-Diagramm aufgeteilt in "Sensoren lesen" (⬜, I²C fehlt noch) + "BMP280-Umrechnung" (✅),
  §5 um has_bmp-Erklärung ergänzt, Landkarten-Tabelle §6 um 3 neue Zeilen, Schluss-Prosa aktualisiert)

# OLED-Sensorstatuszeile (ohne Git) — Plan: docs/superpowers/plans/2026-07-04-oled-sensorstatus.md
Task 1: complete (display_status.h/.cpp: DisplayState + bmp_ok/mpu_ok/ds18b20_ok/uv_ok, status_lines()
  liefert 5. Zeile "B:.. M:.. D:.. U:..", 3 neue Tests + 1 angepasst, RED korrekt gesehen (excess
  struct initializer / fehlende Member), GREEN 9/9 in test_display_status, volle Suite 73/73 grün.
  Review: Spec-Konformität ✅ exakt wie Plan, Code-Qualität approved, nur 2 Minor-Anmerkungen ohne
  Handlungsbedarf (Testname weiterhin "four_lines" trotz jetzt 5 Zeilen — plan-vorgegeben, kein Fehler).)
Task 2: complete (docs/software-flow.md: §3-Absatz auf "fünf Zeilen" + Sensorzeilen-Erklärung, §5 neue
  Box zu den vier *_ok-Flags, §6-Tabelle um 2 Zeilen (Formatierung + main.cpp-Verdrahtung ⬜), Schluss-
  Prosa um Sensorstatuszeile-Absatz ergänzt. Alt-Textstellen im Plan stimmten exakt mit der Datei
  überein, keine Nacharbeit nötig. Review (von mir selbst statt Subagent, nach Nutzer-Interrupt):
  alle 4 Stellen vorhanden, inhaltlich exakt, in sich konsistent, keine "vier Zeilen"-Reste, ⬜-Marker
  korrekt (main.cpp befüllt Flags noch nicht).)
Feature komplett: beide Tasks abgeschlossen, Scope eingehalten (nur lib/telemetry/display_status +
  Doku angefasst, src/flight/main.cpp bewusst nicht verdrahtet — nächster, separater Schritt).
