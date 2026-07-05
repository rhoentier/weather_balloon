# Wetterballon-Elektronik — Kurzüberblick zur Besprechung

**Ziel:** Ein leichtes (< 200 g), günstiges (< 100 €) Elektronik-Paket, das während des
Flugs **Messdaten aufzeichnet** *und* den Ballon per Funk **live verfolgbar + wiederauffindbar**
macht. Flugdauer ca. 2–3 Stunden.

---

## Die Teile und was wir damit erreichen

| Teil | Was es ist | Was wir damit erreichen |
|---|---|---|
| **Heltec WiFi LoRa 32 V2 + NEO-6M (GPS)** ✅ gekauft | Herzstück: Board mit ESP32 (Mini-Computer), LoRa-Funk und OLED-Display in einem, dazu ein separates GPS-Modul | Steuert alles, sendet die Position live zum Boden, weiß wo der Ballon ist und wie hoch er fliegt — **Heltec-Board (ESP32+LoRa) statt T-Beam: günstiger, gut verfügbar, kaum Lötarbeit** |
| **BMP280** ✅ gekauft | Umwelt-Sensor | Misst **Temperatur und Luftdruck** — zeigt den Aufstieg durch die Atmosphäre |
| **MPU-6050** ✅ gekauft | Lage-/Bewegungssensor (6-Achsen) | Misst **Beschleunigung und Drehrate** — zeigt, wie der Ballon schwingt und rotiert (Kompass nicht nötig, da GPS die Richtung liefert) |
| **DS18B20** ✅ gekauft | Außen-Thermometer (am Kabel) | Misst die **echte Außenkälte** (bis ca. −55 °C) getrennt von der warmen Box |
| **GUVA-S12SD** ✅ gekauft | UV-Sensor (analog) | Misst die **UV-Strahlung** (inkl. UV-B) — die mit der Höhe stark zunimmt (unser Experiment) |
| **SPI MicroSD-Modul + Karte** ✅ gekauft | Speicher | Zeichnet **alle Daten lückenlos** auf — zur Auswertung im Unterricht nach dem Flug |
| **4× Lithium-Batterien + Halter** ✅ gekauft | Stromversorgung | Funktionieren auch bei extremer Kälte (−40 °C), halten **über 10 Stunden** |
| **Action-Cam** (Erweiterung) | leichte Kamera, zeichnet **lokal** auf | Filmt den Flug — Bilder/Video **nach der Bergung** ansehen (keine Live-Übertragung, eigenständiges Gerät) |
| **Bodenstation** (2. Funk-Board + Laptop) | Empfänger am Boden | Zeigt die **Position live auf einer Karte** und hilft beim **Wiederfinden** nach der Landung |

---

## Was das System kann (in einfachen Worten)

1. **Messen** — Temperatur (innen + außen), Luftdruck, Höhe, Lage/Bewegung, UV-Strahlung
2. **Speichern** — alles wird sekündlich auf SD-Karte geschrieben (für die Auswertung)
3. **Senden** — die Position geht live per Funk zum Boden (Reichweite zig Kilometer)
4. **Wiederfinden** — nach der Landung sendet der Ballon weiter seine GPS-Position als „Bake"
5. **Filmen** — eine Action-Cam zeichnet den Flug lokal auf (Ansehen nach der Bergung)
    
---

## Kosten & Gewicht (Überblick)

- **Flug-Einheit (ohne Kamera):** ca. **117 g** und ca. **84 €** → unter beiden Limits (< 200 g, < 100 €)
- **+ Action-Cam:** +60–90 g → gesamt ca. **180–210 g** ⚠️ **nahe am 200-g-Limit** → leichtes Cam-Modell wählen + Gesamtgewicht wiegen!
- **Bodenstation:** ca. **25 €** (einmalig, kommt separat dazu)
- Box/Dämmung: schon vorhanden

---

## Bewusst weggelassen (mit Begründung)

- **GSM/Handy-Tracker** — würde Gewicht sprengen; LoRa-Funk reicht zum Wiederfinden
- **Geigerzähler** — zu teuer fürs Budget
- **Luftqualität (CO₂ etc.)** — funktioniert in großer Höhe nicht zuverlässig

---

## Wichtigster Erfolgsfaktor

Das GPS muss per Software in den **„Höhenflug-Modus"** geschaltet werden — sonst schalten viele
GPS-Module über 18 km Höhe ab. Das ist der häufigste Fehler bei Ballonprojekten und wird bei uns
fest eingeplant und **vor dem Flug am Boden getestet**.

