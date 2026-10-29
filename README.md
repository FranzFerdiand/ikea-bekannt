# IKEA Bekant Firmware-Mod mit ESP32/ESP8266 & Heizkörper-Schutz

Dieses Repository enthält den kompletten Quellcode, alle Programmier-Tools und Build-Skripte, um das originale IKEA Bekant Bedienteil (`PIC16LF1938`) ohne teures Spezialgerät mithilfe eines **ESP32 oder ESP8266 als 3,3V Low-Voltage-Programmer** mit einer Memory- & Kollisionsschutz-Firmware zu flashen.

**Keine MPLAB X IDE erforderlich!** Die Firmware ist als fertige `.hex`-Datei bereits enthalten und wird bei Code-Änderungen vollautomatisch über GitHub Actions in der Cloud gebaut.

---

## Projektübersicht & Ordnerstruktur

* [`bin/bekantfirmware.hex`](bin/bekantfirmware.hex): **Fertig kompilierte `.hex`-Datei** (Standard: ca. 73 cm Heizkörperschutz, dynamisch per EEPROM anpassbar, LVP aktiviert).
* [`firmware/`](firmware/): Der Quellcode der Bekant-Firmware (angepasster Fork von `GuentherMair` / `ivanwick`).
  * [`firmware/src/bekant/radiator_config.h`](firmware/src/bekant/radiator_config.h): Definition von Notfall-Minimum, Standardwert (`RADIATOR_DEFAULT_POS`), Bremsreserve und Entsperr-Fenster.
  * [`firmware/src/bekant/bui.c`](firmware/src/bekant/bui.c): Gestensteuerung, dynamisches Heizkörper-Limit aus EEPROM (`low_pos`), Unlock-am-Anschlag-Logik und Relais-Feedback.
  * [`firmware/src/bekant/bctrl.c`](firmware/src/bekant/bctrl.c): Hardwarenaher Sanft-Stopp (LIN-Bus Bremssequenz `0x87 -> 0x84 -> 0xFC`).
  * [`firmware/src/configuration_bits.c`](firmware/src/configuration_bits.c): Konfiguriert mit `LVP = ON` für 3,3V Low-Voltage-Programming via ESP32/ESP8266.
  * [`firmware/Makefile`](firmware/Makefile): Makefile zum direkten Bauen via `xc8-cc`.
* [`.github/workflows/build.yml`](.github/workflows/build.yml): Automatische Cloud-Kompilierung bei jedem Git-Push (GitHub Actions).
* [`tools/esp32_programmer/esp32_pic_programmer.ino`](tools/esp32_programmer/esp32_pic_programmer.ino): Arduino-Sketch für **ESP32** (mit Auto-Erkennung).
* [`tools/esp8266_programmer/esp8266_programmer.ino`](tools/esp8266_programmer/esp8266_programmer.ino): Spezieller Arduino-Sketch für **ESP8266** (NodeMCU / Wemos D1 Mini).
* [`tools/flash_bekant.py`](tools/flash_bekant.py): Python 3 Skript zum Übertragen der `.hex`-Datei über den ESP auf den PIC.
* [`tools/build_local.bat`](tools/build_local.bat): Lokales Batch-Skript zum Bauen per CLI (ohne IDE).

---

## 1. Dynamisches Heizkörper-Minimum im EEPROM (Nie wieder neu flashen!)

Das Heizkörper-Limit ist **dynamisch im EEPROM an `low_pos` gekoppelt**. Du musst den Tisch nach dem ersten Flashen **nie wieder zum Ändern der Mindesthöhe flashen**, sondern kannst das Limit direkt per Tastenkombination am Bedienteil einstellen!

Formel: **1 cm ≈ 100 Ticks** (Standard-Sitzhöhe ca. 70 cm = 1590 Ticks).

$$\text{Ziel-Ticks} = 1590 + (\text{Wunsch-Minimalhöhe in cm} - 70) \times 100$$

Standard-Konfiguration in [`firmware/src/bekant/radiator_config.h`](firmware/src/bekant/radiator_config.h):
```c
#define RADIATOR_UNLOCKED_POS   ((int16_t)0x0190) // Absolutes Notfall-Minimum (400 Ticks ≈ 65 cm)
#define RADIATOR_DEFAULT_POS    ((int16_t)0x0762) // Standard-Startwert leerer Chip (1890 Ticks ≈ 73 cm)
#define RADIATOR_BRAKE_MARGIN   ((int16_t)0x0050) // ca. 8 mm Bremsreserve
#define RADIATOR_UNLOCK_WINDOW  ((int16_t)0x0064) // Toleranzfenster am Anschlag (100 Ticks ≈ 1 cm)
```

---

## 2. Firmware kompilieren (ohne MPLAB X IDE!)

Du musst die ressourcenhungrige MPLAB X IDE **nicht** installieren.

* **Direkt nutzen:** Die fertige Datei [`bin/bekantfirmware.hex`](bin/bekantfirmware.hex) liegt bereits fertig kompiliert im Repository!
* **Nach Codeänderungen (Cloud-Build via GitHub Actions):** Sobald du Änderungen pushst, kompiliert GitHub Actions die Firmware automatisch in einem Linux-Container. Unter dem Reiter **Actions** -> neuester Build findest du das fertige `bekantfirmware-hex` Artifact.
* **Optional lokal per CLI:** Wer den XC8-Compiler auf dem Rechner hat, startet einfach [`tools/build_local.bat`](tools/build_local.bat).

---

## 3. ESP32 oder ESP8266 als Programmer flashen & verkabeln

Beide Mikrocontroller (ESP32 und ESP8266) arbeiten nativ mit 3,3 V Logikpegeln und eignen sich hervorragend:

* **Bei Nutzung eines ESP32:** Flashe [`tools/esp32_programmer/esp32_pic_programmer.ino`](tools/esp32_programmer/esp32_pic_programmer.ino).
* **Bei Nutzung eines ESP8266 (NodeMCU / Wemos D1 Mini):** Flashe [`tools/esp8266_programmer/esp8266_programmer.ino`](tools/esp8266_programmer/esp8266_programmer.ino).

> **SICHERHEIT:** Ziehe vor dem Anschließen das Tischkabel vom Bedienteil ab! Das Bedienteil darf NIEMALS am Tisch eingesteckt sein, während geflasht wird.

### Pinbelegung im Vergleich

| IKEA Bedienteil Pad | Signal | ESP32 Pin | ESP8266 Pin (NodeMCU / D1 Mini) | Wichtiger Hinweis |
| :--- | :--- | :--- | :--- | :--- |
| **Pad 1** (oft eckig) | `MCLR / VPP` | **GPIO 18** | **D5** (GPIO 14) | Reset / Programmierfreigabe |
| **Pad 2** | `VDD` | **3V3** | **3V3** (3.3V) | **ACHTUNG: NIEMALS an 5V / VIN!** |
| **Pad 3** | `VSS` | **GND** | **GND** (G) | Masse |
| **Pad 4** | `PGD` | **GPIO 19** | **D6** (GPIO 12) | ICSP-Daten |
| **Pad 5** | `PGC` | **GPIO 21** | **D7** (GPIO 13) | ICSP-Takt |

---

## 4. Verbindung testen & Flashen

1. Öffne nach dem Flashen des ESP den Arduino Seriellen Monitor (115200 Baud, Newline) und tippe `ID` ein:
   * Erwartete Antwort: `OK ID:0x246x` (Kennung für `PIC16LF1938`).
2. Flashe anschließend die `.hex`-Datei über Python:
```bash
python tools/flash_bekant.py COM3 bin/bekantfirmware.hex
```
*(Ersetze `COM3` durch deinen COM-Port).*

---

## 5. Bedienung im Alltag

* **Automatisches Fahren (Gestensteuerung):**
  * Nach **OBEN:** `Hoch (△)` halten, kurz `Runter (▽)` tippen, loslassen.
  * Nach **UNTEN:** `Runter (▽)` halten, kurz `Hoch (△)` tippen, loslassen.
  * **Sofort-Stopp:** Beliebige Taste antippen.

### Heizkörper-Limit und Speicherpositionen verwalten

| Was du tun möchtest | Vorgehen am Tisch |
| :--- | :--- |
| **Heizkörper-Minimum zum ersten Mal festlegen** | Auf Wunschhöhe über den Heizkörper fahren $\rightarrow$ **3 Sek. beide Tasten halten** (speichert `low_pos` als Sitzhöhe & Hard-Limit). Danach nach oben fahren $\rightarrow$ **3 Sek. beide Tasten halten** (speichert `high_pos`). |
| **Minimum später nach OBEN korrigieren** | Einfach auf die neue (höhere) Position fahren $\rightarrow$ **3 Sek. beide Tasten halten**. |
| **Minimum später nach UNTEN korrigieren** *(Unlock am Anschlag)* | 1. Ganz nach unten bis zum automatischen Stopp am aktuellen Limit fahren.<br>2. Direkt am Stopp **3 Sek. beide Tasten halten** *(Relais klicken als Bestätigung, Limit ist nun temporär entsperrt)*.<br>3. Mit `Runter` vorsichtig tiefer auf die neue Höhe fahren.<br>4. Erneut **3 Sek. beide Tasten halten** *(neues Limit ist fest im EEPROM verankert & wieder aktiv)*. |
| **Werkseinstellungen wiederherstellen** | Beide Tasten **8 Sekunden** gedrückt halten (Relais klickt, alle Speicherpositionen im EEPROM werden gelöscht). |
