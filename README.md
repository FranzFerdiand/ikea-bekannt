# IKEA Bekant Firmware-Mod mit ESP32 & Heizkörper-Schutz

Dieses Projekt enthält alle benötigten Skripte, Quellcode-Modifikationen und Dokumentationen, um das originale IKEA Bekant Bedienteil (`PIC16LF1938`) ohne teures PICkit mithilfe eines **ESP32 als 3,3V Low-Voltage-Programmer** mit einer Memory- & Kollisionsschutz-Firmware zu flashen.

---

## Projektübersicht & Ordnerstruktur

* [**`BekantFirmware/`**](https://github.com/FranzFerdiand/BekantFirmware): Der persönliche Fork von [`GuentherMair/BekantFirmware`](https://github.com/GuentherMair/BekantFirmware), als Git-Submodule eingebunden.
  * [`BekantFirmware/src/bekant/radiator_config.h`](https://github.com/FranzFerdiand/BekantFirmware/blob/master/src/bekant/radiator_config.h): Definition von `RADIATOR_MIN_POS` und Bremsreserve.
  * [`BekantFirmware/src/bekant/bui.c`](https://github.com/FranzFerdiand/BekantFirmware/blob/master/src/bekant/bui.c): Verhindert Abwärtsfahrt unter das Limit und blockiert Speichern unzulässiger Memory-Positionen.
  * [`BekantFirmware/src/bekant/bctrl.c`](https://github.com/FranzFerdiand/BekantFirmware/blob/master/src/bekant/bctrl.c): Hardwarenaher Sanft-Stopp (LIN-Bus Bremssequenz `0x87 -> 0x84 -> 0xFC`).
  * [`BekantFirmware/src/configuration_bits.c`](https://github.com/FranzFerdiand/BekantFirmware/blob/master/src/configuration_bits.c): Konfiguriert mit `LVP = ON` für 3,3V ESP32 Flashen.
* [`tools/esp32_programmer/esp32_pic_programmer.ino`](tools/esp32_programmer/esp32_pic_programmer.ino): Arduino-Sketch für den ESP32 (implementiert Microchip DS41397 LVP Protokoll).
* [`tools/flash_bekant.py`](tools/flash_bekant.py): Python 3 Flashing-Tool zum Übertragen der `.hex`-Datei über den ESP32 auf den PIC.

---

## 1. Minimalhöhe für deinen Heizkörper konfigurieren

Formel: **1 cm ≈ 100 Ticks** (Standard-Sitzhöhe ca. 70 cm = 1590 Ticks).

$$\text{Ziel-Ticks} = 1590 + (\text{Wunsch-Minimalhöhe in cm} - 70) \times 100$$

Passe bei Bedarf die Werte in [`BekantFirmware/src/bekant/radiator_config.h`](https://github.com/FranzFerdiand/BekantFirmware/blob/master/src/bekant/radiator_config.h) an:
```c
#define RADIATOR_MIN_POS        ((int16_t)0x0762) // ca. 73 cm Tischhöhe
#define RADIATOR_BRAKE_MARGIN   ((int16_t)0x0050) // ca. 8 mm Bremsreserve
```

---

## 2. Firmware kompilieren (.hex erstellen)

1. Öffne **MPLAB X IDE**.
2. Wähle **File -> Open Project** und öffne das Projekt bzw. erstelle ein neues *Standalone Project* mit Gerät `PIC16LF1938` und Compiler `XC8`.
3. Binde die C-Dateien aus `BekantFirmware/src/` ein.
4. Klicke auf **Clean and Build**. Die erzeugte `.hex`-Datei liegt unter:
   `dist/default/production/...hex`

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

Prüfe im Arduino Seriellen Monitor (115200 Baud) mit dem Befehl `ID`:
* Erwartete Antwort: `OK ID:0x246x` (Kennung für `PIC16LF1938`).

Flashe anschließend die `.hex`-Datei via Python:
```bash
python tools/flash_bekant.py COM3 pfad/zu/bekantfirmware.hex
```

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
