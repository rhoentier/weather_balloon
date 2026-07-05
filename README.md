# Wetterballon — PlatformIO-Befehle

Schul-Wetterballon mit ESP32 (Heltec WiFi LoRa 32 V2). Diese README ist die
Schnellreferenz für den Umgang mit PlatformIO. Projekt-Details stehen in
[`CLAUDE.md`](CLAUDE.md), die Schritte in [`TODO.md`](TODO.md).

---

## Einmalig: PlatformIO im Terminal verfügbar machen

Die PlatformIO-CLI liegt unter `~/.platformio/penv/bin/`. Damit `pio` im
Terminal gefunden wird, den Ordner zum PATH hinzufügen:

```bash
export PATH="$PATH:$HOME/.platformio/penv/bin"
```

Das gilt nur für die aktuelle Terminal-Sitzung. Dauerhaft: die Zeile ans Ende
von `~/.zshrc` schreiben. Alternativ die PlatformIO-Buttons in VS Code nutzen —
dann braucht man den PATH nicht.

Prüfen, ob es klappt:

```bash
pio --version
```

---

## Die zwei Environments

Ein Repo, zwei Ziele (definiert in `platformio.ini`):

| Environment | Ziel | Board |
|---|---|---|
| `flight`  | Firmware der **Flug-Einheit**   | Heltec WiFi LoRa 32 **V2** |
| `ground`  | Firmware der **Bodenstation**   | Heltec WiFi LoRa 32 **V3** |

`-e <name>` wählt das Environment aus. Ohne `-e` wird `flight` genommen
(`default_envs`).

---

## Die wichtigsten Befehle

### Flug-Firmware kompilieren

Übersetzt den Code, flasht aber noch nicht. Gut, um Fehler früh zu sehen:

```bash
pio run -e flight
```

### Aufs Board flashen

Board **per USB** anschließen, dann:

```bash
pio run -e flight -t upload
```

### Serielle Ausgabe ansehen

Zeigt, was das Board über USB ausgibt (115200 Baud). Beenden mit `Strg+C`:

```bash
pio device monitor
```

Tipp: Flashen + Monitor in einem Schritt:

```bash
pio run -e flight -t upload -t monitor
```

### Bodenstation

Dieselben Befehle, nur mit `-e ground`:

```bash
pio run -e ground -t upload
```

---

## Aufräumen bei komischen Build-Fehlern

Wenn ein Build unerklärlich fehlschlägt (oft nach Umbauten an
`platformio.ini`), die Build-Artefakte löschen und neu bauen:

```bash
pio run -e flight -t clean
```

---

## Stolperfallen (board-spezifisch)

- **Upload klemmt / „Failed to connect":** Beim Heltec V2 beim Start des
  Flashens ggf. den **PRG-Button** gedrückt halten, bis der Upload anläuft.
- **Falsches Board flashen:** `flight` = V2, `ground` = V3 — **nicht
  vertauschen**. Die Boards haben unterschiedliche Chips und Pinbelegungen.
- **Kein Gerät gefunden:** `pio device list` zeigt die erkannten seriellen
  Ports. Erscheint nichts, fehlt meist der USB-Treiber (CP2102) oder das Kabel
  ist ein reines Ladekabel ohne Datenleitungen.
- **Zum Testen immer USB, keine Batterie** — Begründung siehe `CLAUDE.md`
  (Batteriespannung am 5V-Pin ist ein offener Punkt).
