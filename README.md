# IKEA Bekant Firmware-Mod mit ESP32 / ESP8266 & dynamischem Heizkörper-Schutz

Dieses Repository enthält den kompletten Quellcode, alle Programmier-Tools und Build-Skripte, um das originale IKEA Bekant Bedienteil (`PIC16LF1938`) **ohne teures Spezialgerät (wie PICkit)** mithilfe eines gewöhnlichen **ESP32 oder ESP8266 als 3,3V ICSP-Programmer** mit einer Memory- & Kollisionsschutz-Firmware zu flashen.

**Keine MPLAB X IDE erforderlich!** Die Firmware liegt als fertige `.hex`-Datei bereit und wird bei Code-Änderungen vollautomatisch über GitHub Actions in der Cloud gebaut.

---

## 🌟 Highlights & Features

1. **Kein Spezialprogrammer nötig:** Jeder ESP32 oder ESP8266 (NodeMCU / D1 Mini) flasht den Chip direkt über 3,3 V.
2. **Rock-Solides ICSP-Protokoll (Microchip DS41397B):** 32-Wort-Zeilenprogrammierung (`WROW32`) und gezielte Konfigurationsprogrammierung (`WCONFIG`) flashen den gesamten PIC in unter **20 Sekunden**.
3. **9V-Batterie-Trick:** Entsperrt beim allerersten Mal die werkseitige Sperre (`LVP = OFF`), danach reicht reines 3,3V-LVP für immer aus.
4. **Dynamisches Heizkörper-Limit im EEPROM:** Die untere Stoppgrenze (`low_pos`) ist direkt an das EEPROM gekoppelt. Du musst den Tisch **nie wieder neu flashen**, um das Limit zu verstellen!
5. **Notfall-Unlock am Anschlag:** Wer doch einmal tiefer fahren muss, hält die Tasten am Anschlag für 3 Sekunden gedrückt, um das Limit temporär aufzuheben.
6. **Dual-Gestensteuerung:** Unterstützt sowohl die intuitive 1-Hand-Doppelklick-Steuerung als auch die klassische 2-Hand-Kombinationssteuerung.

---

## 📁 Repository-Struktur

* [`bin/bekantfirmware.hex`](bin/bekantfirmware.hex): **Fertig kompilierte Firmware** (Standard: ca. 73 cm Heizkörperschutz, dynamisch per EEPROM anpassbar, `LVP = ON`).
* [`tools/esp32_programmer/esp32_pic_programmer.ino`](tools/esp32_programmer/esp32_pic_programmer.ino): Universeller Arduino-Sketch für **ESP32** (Pin GPIO 18, 19, 21).
* [`tools/esp8266_programmer/esp8266_programmer.ino`](tools/esp8266_programmer/esp8266_programmer.ino): Arduino-Sketch für **ESP8266** (NodeMCU / Wemos D1 Mini; Pin D5, D6, D7).
* [`tools/flash_bekant.py`](tools/flash_bekant.py): High-Speed Python-Flasher mit 32-Wort-Zeilenspeicher und automatischer Verifikation.
* [`tools/check_diag.py`](tools/check_diag.py): Diagnose-Skript zum Auslesen von Device ID, CONFIG1, CONFIG2 und Reset-Vektoren.
* [`firmware/`](firmware/): Quellcode der Bekant-Firmware:
  * [`src/bekant/radiator_config.h`](firmware/src/bekant/radiator_config.h): Konfiguration von Minimalgrenzen, Bremsreserve und Unlock-Toleranz.
  * [`src/bekant/bui.c`](firmware/src/bekant/bui.c): UI-Logik, Gesten, dynamischer EEPROM-Speicher und Relais-Rückmeldung.
  * [`src/bekant/bctrl.c`](firmware/src/bekant/bctrl.c): LIN-Bus Steuerung und Bremssequenz (`0x87 -> 0x84 -> 0xFC`).
  * [`src/configuration_bits.c`](firmware/src/configuration_bits.c): PIC-Fuses mit `LVP = ON` und `FOSC = INTOSC`.
* [`.github/workflows/build.yml`](.github/workflows/build.yml): Automatische Cloud-Kompilierung via GitHub Actions.

---

## 🔌 Verkabelung & Hardware

> [!CAUTION]
> **SICHERHEIT:** Ziehe vor dem Anschließen des ESP **immer das Verbindungskabel zum Tischnetzteil ab!** Das Bedienteil darf während des Flash-Vorgangs **niemals** am Tisch eingesteckt sein.

### Pinbelegung Bedienteil <-> ESP

| IKEA Bedienteil Pad | Signal | ESP32 Pin | ESP8266 Pin (NodeMCU / D1 Mini) | Wichtiger Hinweis |
| :--- | :--- | :--- | :--- | :--- |
| **Pad 1** (oft eckig) | `MCLR / VPP` | **GPIO 18** | **D5** (GPIO 14) | Reset / Programmierfreigabe |
| **Pad 2** | `VDD` | **3V3** | **3V3** (3.3V) | **ACHTUNG: NIEMALS an 5V / VIN!** |
| **Pad 3** | `VSS` | **GND** | **GND** (G) | Masse |
| **Pad 4** | `PGD` | **GPIO 19** | **D6** (GPIO 12) | ICSP-Daten (ICSPDAT) |
| **Pad 5** | `PGC` | **GPIO 21** | **D7** (GPIO 13) | ICSP-Takt (ICSPCLK) |

---

## ⚡ Der 9V-Batterie-Trick beim ALLERERSTEN Flashen

IKEA liefert den `PIC16LF1938` ab Werk mit gesetzter Sicherung **`LVP = OFF`** (Low-Voltage-Programming deaktiviert) aus. Dadurch reagiert ein fabrikneuer Chip zunächst nicht auf reine 3,3V-Befehle am MCLR-Pin.

Mit einer gewöhnlichen **9V-Blockbatterie** lässt sich diese Sperre in Sekunden überlisten:
1. **Batterie-Minus (GND):** Verbinde den Minuspol der 9V-Batterie mit **GND** (ESP GND bzw. Pad 3).
2. **Batterie-Plus (+9V):**
   * Halte beim ersten Flashen den Pluspol der 9V-Batterie (idealerweise über einen 1 kΩ – 10 kΩ Widerstand) an **Pad 1 (MCLR)**, während der Erase- und Flash-Vorgang startet.
   * *Alternativ:* Pin D5 / GPIO 18 über eine kleine Diode (z. B. 1N4148, Kathode an Pad 1) verbinden, damit keine 9 V in den ESP fließen.
3. Sobald der Chip einmal gelöscht und unsere Firmware geflasht wurde, ist **`LVP = ON` dauerhaft aktiv**!
4. **Ab dem 2. Flashen:** Du brauchst die 9V-Batterie nie wieder! Pin D5 / GPIO 18 steuert den Chip vollautomatisch mit 3,3 V.

---

## 🚀 Schritt-für-Schritt Anleitung

### Schritt 1: ESP-Programmer flashen
1. Öffne die Arduino IDE.
2. Lade [`tools/esp32_programmer/esp32_pic_programmer.ino`](tools/esp32_programmer/esp32_pic_programmer.ino) (für ESP32) bzw. [`tools/esp8266_programmer/esp8266_programmer.ino`](tools/esp8266_programmer/esp8266_programmer.ino) (für ESP8266).
3. Flashe den Sketch auf deinen ESP.

### Schritt 2: Firmware flashen
Verbinde die 5 Leitungen mit dem ausgesteckten Bedienteil und führe auf dem PC aus:

```bash
python tools/flash_bekant.py COM12 bin/bekantfirmware.hex
```
*(Ersetze `COM12` durch deinen Port).*

**Ablauf im Skript:**
* Erkennt den Chip (`OK ID:0x24A3`).
* Löscht Flash & Konfiguration (`OK ERASED`).
* Schreibt alle 128 Zeilen per 32-Wort-Latch (`WROW32`).
* Schreibt `CONFIG1 = 0x0FFC` und `CONFIG2 = 0x3AFC`.
* Verifiziert alle Schlüsseladressen. Dauer: ca. **15–20 Sekunden**!

### Schritt 3: Schnelle Diagnose (optional)
Mit [`tools/check_diag.py`](tools/check_diag.py) kannst du den Speicher jederzeit überprüfen:
```bash
python tools/check_diag.py COM12
```

---

## 🛠️ Wichtig beim Wiederanstecken an den Tisch (Troubleshooting)

> [!IMPORTANT]
> **Power-Cycle durchführen!**
> Wenn das Bedienteil nach dem Flashen wieder an den Tisch angesteckt wird und der Tisch zunächst gar nicht reagiert:
> 1. Trenne das Tischnetzteil für **15 Sekunden komplett vom 230V-Netz**.
> 2. Stecke es wieder ein.
> 
> **Hintergrund:** Beim allerersten Start führt die Firmware einen LIN-Bus-Scan durch (`bscan_scan()`), um die Tischbeine zu initialisieren. Waren die Beine noch im alten Status oder der Bus kurzzeitig blockiert (Messung Weiß–Blau liegt bei ~2V statt >20V), schalten die Beine stumm. Erst ein kompletter Kaltstart synchronisiert Controller und Beine sauber.

---

## 🎮 Bedienung & Gestensteuerung

### Normales Fahren & Memory-Positionen
* **Hochfahren:** Taste `▲` drücken und halten.
* **Runterfahren:** Taste `▼` drücken und halten.
* **Auto-Fahrt zur Sitzposition (Memory 1):** Doppelklick auf `▼` *oder* `▼` halten und kurz `▲` tippen.
* **Auto-Fahrt zur Stehposition (Memory 2):** Doppelklick auf `▲` *oder* `▲` halten und kurz `▼` tippen.
* **Sofort-Stopp:** Beliebige Taste kurz antippen.

### Heizkörper-Limit (unterer Endstopp) anpassen

| Aktion | Vorgehensweise am Bedienteil |
| :--- | :--- |
| **Heizkörper-Limit erstmalig festlegen** | Auf gewünschte Mindesthöhe fahren $\rightarrow$ **3 Sekunden beide Tasten halten** (Relais klickt: `low_pos` ist gespeichert und bildet die neue Untergrenze). |
| **Limit nach OBEN korrigieren** | Einfach auf die neue, höhere Position fahren $\rightarrow$ **3 Sekunden beide Tasten halten**. |
| **Limit nach UNTEN korrigieren (Unlock am Anschlag)** | 1. Ganz nach unten bis zum automatischen Stopp am aktuellen Limit fahren.<br>2. Direkt am Stopp **3 Sekunden beide Tasten halten** (Relais klickt: Limit ist temporär entsperrt).<br>3. Mit `▼` vorsichtig tiefer auf die neue Höhe fahren.<br>4. Erneut **3 Sekunden beide Tasten halten** (neues Limit ist fest gespeichert). |
| **Werkseinstellungen wiederherstellen** | Beide Tasten für **8 Sekunden** gedrückt halten (alle Speicherpositionen im EEPROM werden gelöscht). |

---

## 🙏 Credits & Danksagung

Dieses Projekt basiert auf der fantastischen Vorarbeit und Dokumentation der Open-Source-Community:

* **[ivanwick (Ivan Wick)](https://github.com/ivanwick/bekantfirmware):** Urheber des ursprünglichen Reverse-Engineerings des IKEA Bekant LIN-Bus-Protokolls, der ersten freien Firmware-Architektur und der Dokumentation der Platinen-Pinouts.
* **[DieSteinhose](https://github.com/DieSteinhose/bekantfirmware):** Urheber der 1-Hand-Doppelklick-Gestensteuerung für das Bekant-Bedienteil.
* **[GuentherMair](https://github.com/GuentherMair/bekantfirmware):** Upstream-Dokumentation und kontinuierliche Pflege des Firmware-Cores.
* **Microchip Technology:** Für die offizielle ICSP-Programmierspezifikation DS41397B für die PIC16(L)F193X-Familie.
* **iFixit & Community-Tester:** Für detaillierte Gehäuse-Teardowns und wertvolle Messungen der Hardware-Revisionen.

---

## 📄 Lizenz

Dieses Projekt steht unter der gleichen Open-Source-Lizenz wie das Basis-Projekt (siehe [firmware/LICENSE](firmware/LICENSE)).
Modifikationen für Heizkörper-Kollisionsschutz, EEPROM-Dynamik und ESP32/ESP8266-Programmer-Tools sind frei zur Nutzung, Weiterentwicklung und Veröffentlichung.
