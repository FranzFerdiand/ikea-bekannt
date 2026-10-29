# IKEA Bekant Firmware-Mod mit ESP32 & Heizkörper-Schutz

Dieses Repository enthält den kompletten Quellcode, alle Programmier-Tools und Build-Skripte, um das originale IKEA Bekant Bedienteil (`PIC16LF1938`) ohne teures Spezialgerät mithilfe eines **ESP32 als 3,3V Low-Voltage-Programmer** mit einer Memory- & Kollisionsschutz-Firmware zu flashen.

**Keine MPLAB X IDE erforderlich!** Die Firmware kann vollautomatisch über GitHub Actions gebaut oder per CLI-Skript kompiliert werden.

---

## Projektübersicht & Ordnerstruktur

* [`firmware/`](firmware/): Der Quellcode der Bekant-Firmware (angepasster Fork von `GuentherMair` / `ivanwick`).
  * [`firmware/src/bekant/radiator_config.h`](firmware/src/bekant/radiator_config.h): Definition von `RADIATOR_MIN_POS` (Heizkörperhöhe) und Bremsreserve.
  * [`firmware/src/bekant/bui.c`](firmware/src/bekant/bui.c): Gestensteuerung, Schutz vor Abwärtsfahrt unter das Limit und Blockade unzulässiger Memory-Speicherwerte.
  * [`firmware/src/bekant/bctrl.c`](firmware/src/bekant/bctrl.c): Hardwarenaher Sanft-Stopp (LIN-Bus Bremssequenz `0x87 -> 0x84 -> 0xFC`).
  * [`firmware/src/configuration_bits.c`](firmware/src/configuration_bits.c): Konfiguriert mit `LVP = ON` für 3,3V Low-Voltage-Programming via ESP32.
  * [`firmware/Makefile`](firmware/Makefile): Makefile zum direkten Bauen der `.hex`-Datei via `xc8-cc`.
* [`.github/workflows/build.yml`](.github/workflows/build.yml): Automatische Cloud-Kompilierung bei jedem Git-Push (erzeugt die fertige `.hex`-Datei zum Download).
* [`tools/esp32_programmer/esp32_pic_programmer.ino`](tools/esp32_programmer/esp32_pic_programmer.ino): Arduino-Sketch für den ESP32 (implementiert das Microchip DS41397 LVP ICSP-Protokoll).
* [`tools/flash_bekant.py`](tools/flash_bekant.py): Python 3 Skript zum Übertragen der `.hex`-Datei über den ESP32 auf den PIC.
* [`tools/build_local.bat`](tools/build_local.bat): Lokales Batch-Skript zum Bauen per CLI (ohne IDE).

---

## 1. Minimalhöhe für deinen Heizkörper konfigurieren

Formel: **1 cm ≈ 100 Ticks** (Standard-Sitzhöhe ca. 70 cm = 1590 Ticks).

$$\text{Ziel-Ticks} = 1590 + (\text{Wunsch-Minimalhöhe in cm} - 70) \times 100$$

Passe bei Bedarf die Werte in [`firmware/src/bekant/radiator_config.h`](firmware/src/bekant/radiator_config.h) an:
```c
#define RADIATOR_MIN_POS        ((int16_t)0x0762) // ca. 73 cm Tischhöhe (1890 Ticks)
#define RADIATOR_BRAKE_MARGIN   ((int16_t)0x0050) // ca. 8 mm Bremsreserve
```

---

## 2. Firmware kompilieren (.hex erstellen – ohne MPLAB X IDE!)

Du musst die ressourcenhungrige MPLAB X IDE **nicht** installieren. Es gibt zwei einfache Wege:

### Weg A: Vollautomatisch über GitHub Actions (Empfohlen – 0 Installationen)
1. Wenn du Änderungen im Repository pushst (oder auf GitHub unter dem Tab **Actions** auf *Run workflow* klickst), baut GitHub die Firmware automatisch in einem Linux-Container.
2. Gehe auf GitHub auf den Reiter **Actions** -> neuester Build -> lade unter **Artifacts** die Datei `bekantfirmware-hex` herunter.
3. Entpacke die `.hex`-Datei in deinen Projektordner.

### Weg B: Per Skript auf dem PC (rein über Kommandozeile)
Falls du den XC8-Compiler lieber lokal auf dem Rechner haben möchtest:
1. Installiere lediglich den kostenlosen [MPLAB XC8 Compiler](https://www.microchip.com/en-us/tools-resources/develop/microchip-mplab-xc-compilers) (ohne die IDE).
2. Führe im Ordner `tools/` einfach [`build_local.bat`](tools/build_local.bat) aus.
3. Die fertige Datei wird in `firmware/dist/bekantfirmware.hex` abgelegt.

---

## 3. ESP32 als Programmer flashen & verkabeln

1. Öffne die **Arduino IDE**, wähle dein ESP32 Board und flashe [`tools/esp32_programmer/esp32_pic_programmer.ino`](tools/esp32_programmer/esp32_pic_programmer.ino).
2. **SICHERHEIT:** Ziehe das Tischkabel vom Bedienteil ab! Das Bedienteil darf NIEMALS am Tisch eingesteckt sein, während geflasht wird.
3. Verbinde die 5 ICSP-Pads des IKEA Bedienteils mit dem ESP32:

| IKEA ICSP Pad | Signal | ESP32 Pin | Hinweis |
| :--- | :--- | :--- | :--- |
| **Pad 1** (oft eckig) | `MCLR / VPP` | **GPIO 18** | Reset / VPP |
| **Pad 2** | `VDD` | **3V3** | **ACHTUNG: NIEMALS an 5V / VIN!** |
| **Pad 3** | `VSS` | **GND** | Masse |
| **Pad 4** | `PGD` | **GPIO 19** | ICSPDAT |
| **Pad 5** | `PGC` | **GPIO 21** | ICSPCLK |

---

## 4. Verbindung testen & Flashen

1. Prüfe im Arduino Seriellen Monitor (115200 Baud) mit dem Befehl `ID`:
   * Erwartete Antwort: `OK ID:0x246x` (Kennung für `PIC16LF1938`).
2. Flashe anschließend die `.hex`-Datei über Python:
```bash
python tools/flash_bekant.py COM3 firmware/dist/bekantfirmware.hex
```
*(Ersetze `COM3` durch deinen COM-Port).*

---

## 5. Bedienung im Alltag

* **Automatisches Fahren (Gestensteuerung):**
  * Nach **OBEN:** `Hoch (△)` halten, kurz `Runter (▽)` tippen, loslassen.
  * Nach **UNTEN:** `Runter (▽)` halten, kurz `Hoch (△)` tippen, loslassen.
  * **Sofort-Stopp:** Beliebige Taste antippen.
* **Höhenposition speichern:**
  * Wunschhöhe anfahren. Beide Tasten 3 Sekunden gedrückt halten (Relais klickt).
* **Heizkörper-Schutz:**
  * Der Tisch stoppt bei Erreichen von `RADIATOR_MIN_POS` automatisch und sanft – auch wenn die Runter-Taste manuell gedrückt bleibt!
