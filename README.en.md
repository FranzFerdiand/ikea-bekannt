[🇩🇪 Deutsch](README.md) | [🇬🇧 English](README.en.md)

# IKEA Bekant Firmware Mod with ESP32 / ESP8266 & Dynamic Radiator Collision Protection

This repository contains the complete source code, programmer tools, and build scripts to flash the original IKEA Bekant desk controller (`PIC16LF1938`) **without expensive specialized hardware (such as a PICkit)** using a common **ESP32 or ESP8266 as a 3.3V ICSP programmer** with custom position-memory and collision-protection firmware.

**No MPLAB X IDE required!** Precompiled `.hex` firmware is readily available in `bin/` and is automatically rebuilt in the cloud on every push via GitHub Actions.

---

## 🌟 Highlights & Features

1. **No Dedicated PIC Programmer Required:** Any standard ESP32 or ESP8266 (NodeMCU / D1 Mini) flashes the microcontroller directly via 3.3V ICSP.
2. **Rock-Solid ICSP Implementation (Microchip DS41397B):** 32-word row programming (`WROW32`) and dedicated configuration writes (`WCONFIG`) flash the entire chip in under **20 seconds**.
3. **9V Battery Trick for Initial Flash:** Bypass factory-enabled code protection (`LVP = OFF`) on the very first flash; once programmed, standard 3.3V LVP is permanently active.
4. **Dynamic Radiator Limit in EEPROM:** The minimum height threshold (`low_pos`) is saved dynamically in EEPROM. You **never need to reflash the firmware** to adjust your lower desk limit!
5. **Emergency Endstop Unlock Gesture:** If you ever need to drive lower than your configured limit, holding both buttons for 3 seconds directly at the stop temporarily unlocks downward motion.
6. **Dual Gesture Control:** Supports both 1-hand double-tap gestures and classic 2-hand combination presses out of the box.

---

## 📁 Repository Structure

* [`bin/bekantfirmware.hex`](bin/bekantfirmware.hex): **Precompiled binary** (default: ~73 cm radiator collision limit, dynamically adjustable via EEPROM, `LVP = ON`).
* [`tools/esp32_programmer/esp32_pic_programmer.ino`](tools/esp32_programmer/esp32_pic_programmer.ino): Universal Arduino sketch for **ESP32** (pins GPIO 18, 19, 21).
* [`tools/esp8266_programmer/esp8266_programmer.ino`](tools/esp8266_programmer/esp8266_programmer.ino): Arduino sketch for **ESP8266** (NodeMCU / Wemos D1 Mini; pins D5, D6, D7).
* [`tools/flash_bekant.py`](tools/flash_bekant.py): High-speed Python flasher with 32-word row buffering and automatic readback verification.
* [`tools/check_diag.py`](tools/check_diag.py): Diagnostic utility to read Device ID, CONFIG1, CONFIG2, and reset vectors.
* [`firmware/`](firmware/): Source code and technical documentation:
  * [`src/bekant/radiator_config.h`](firmware/src/bekant/radiator_config.h): Configuration limits, braking margin, and unlock tolerance.
  * [`src/bekant/bui.c`](firmware/src/bekant/bui.c): UI logic, gestures, dynamic EEPROM persistence, and relay feedback.
  * [`src/bekant/bctrl.c`](firmware/src/bekant/bctrl.c): LIN-bus motor control and soft-stop sequence (`0x87 -> 0x84 -> 0xFC`).
  * [`src/configuration_bits.c`](firmware/src/configuration_bits.c): PIC fuse bits configured with `LVP = ON` and internal oscillator (`FOSC = INTOSC`).
  * [`docs/`](firmware/docs/): Technical documentation (architecture, upstream gesture comparison, endstop analysis, state traces).
* [`.github/workflows/build.yml`](.github/workflows/build.yml): Automated cloud build workflow using GitHub Actions.

---

## 🔌 Hardware Wiring & Pinout

> [!CAUTION]
> **SAFETY FIRST:** Always **disconnect the desk power supply cable from the controller before connecting the ESP!** Never connect programmer wires while the controller is plugged into mains power.

### Pin Mapping: Controller <-> ESP

| IKEA Controller Pad | Signal | ESP32 Pin | ESP8266 Pin (NodeMCU / D1 Mini) | Note |
| :--- | :--- | :--- | :--- | :--- |
| **Pad 1** (square pad) | `MCLR / VPP` | **GPIO 18** | **D5** (GPIO 14) | Reset / High Voltage / Program Entry |
| **Pad 2** | `VDD` | **3V3** | **3V3** (3.3V) | **CAUTION: NEVER connect to 5V or VIN!** |
| **Pad 3** | `VSS` | **GND** | **GND** (G) | Ground |
| **Pad 4** | `PGD` | **GPIO 19** | **D6** (GPIO 12) | ICSP Data (`ICSPDAT`) |
| **Pad 5** | `PGC` | **GPIO 21** | **D7** (GPIO 13) | ICSP Clock (`ICSPCLK`) |

---

## ⚡ The 9V Battery Trick (Initial Flash Only)

Factory IKEA controllers ship with the `PIC16LF1938` configured with **`LVP = OFF`** (Low-Voltage Programming disabled). Fresh out of the box, the chip does not enter programming mode on 3.3V alone.

A standard **9V alkaline battery** solves this in seconds without expensive programmers:
1. **Battery Negative (GND):** Connect battery (-) to **GND** (ESP GND or Pad 3).
2. **Battery Positive (+9V):**
   * During the initial erase/flash command, apply +9V (ideally via a 1 kΩ – 10 kΩ current-limiting resistor) to **Pad 1 (MCLR)**.
   * *Alternatively:* Connect Pin D5 / GPIO 18 through a small diode (e.g. 1N4148, cathode facing Pad 1) to protect the microcontroller from 9V backfeed.
3. Once the chip is bulk-erased and flashed with this firmware, **`LVP = ON` is permanently enabled**!
4. **Subsequent Flashes:** You will never need the 9V battery again. Pin D5 / GPIO 18 controls MCLR automatically at 3.3V.

---

## 🚀 Step-by-Step Flashing Guide

### Step 1: Program the ESP
1. Open the Arduino IDE.
2. Open [`tools/esp32_programmer/esp32_pic_programmer.ino`](tools/esp32_programmer/esp32_pic_programmer.ino) (ESP32) or [`tools/esp8266_programmer/esp8266_programmer.ino`](tools/esp8266_programmer/esp8266_programmer.ino) (ESP8266).
3. Upload the sketch to your board.

### Step 2: Flash the Bekant Firmware
Wire the 5 lines to the disconnected controller and run on your computer:

```bash
python tools/flash_bekant.py COM12 bin/bekantfirmware.hex
```
*(Replace `COM12` with your serial port).*

**What happens:**
* Detects chip (`OK ID:0x24A3`).
* Erases program flash and configuration (`OK ERASED`).
* Writes 128 rows using 32-word latches (`WROW32`).
* Programs `CONFIG1 = 0x0FFC` and `CONFIG2 = 0x3AFC`.
* Verifies critical addresses. Total flashing duration: **15–20 seconds**!

### Step 3: Run Diagnostic Check (Optional)
Run [`tools/check_diag.py`](tools/check_diag.py) to inspect the memory anytime:
```bash
python tools/check_diag.py COM12
```

---

## 🛠️ Reconnecting to Desk (Important Troubleshooting)

> [!IMPORTANT]
> **Perform a Complete Power Cycle!**
> When reconnecting the modified controller to the desk, if the desk initially does not move:
> 1. Unplug the desk power brick from mains for **15 seconds**.
> 2. Plug it back in.
> 
> **Why:** On boot, the firmware runs a LIN-bus scan (`bscan_scan()`) to discover both motorized legs. If the legs were in a previous timeout state or the bus experienced contention, the leg controllers stay silent until a full power cycle resets their internal state.

---

## 🎮 Button Controls & Gestures

### Moving & Recalling Stored Heights
* **Move Up:** Press and hold `▲`.
* **Move Down:** Press and hold `▼`.
* **Auto-Drive to Sitting Height (Memory 1):** Double-tap `▼` *or* hold `▼` and briefly tap `▲`.
* **Auto-Drive to Standing Height (Memory 2):** Double-tap `▲` *or* hold `▲` and briefly tap `▼`.
* **Immediate Stop:** Tap either button during automatic movement.

### Adjusting Radiator Limit (Lower Endstop)

| Action | How to perform on the controller |
| :--- | :--- |
| **Set New Lower Limit** | Move to desired minimum height $\rightarrow$ **Hold both buttons for 3 seconds** (relay clicks: current position is saved in EEPROM as the lower boundary). |
| **Adjust Limit UPWARDS** | Move up to the higher target position $\rightarrow$ **Hold both buttons for 3 seconds**. |
| **Adjust Limit DOWNWARDS (Endstop Unlock)** | 1. Move down until the desk automatically halts at the existing limit.<br>2. While stopped at the limit, **hold both buttons for 3 seconds** (relay clicks: limit is temporarily unlocked).<br>3. Carefully drive lower with `▼`.<br>4. **Hold both buttons for 3 seconds** again to lock in the new lower limit. |
| **Factory Reset** | Hold both buttons continuously for **8 seconds** (all saved heights and limits are cleared from EEPROM). |

---

## 💡 Ergonomics Guide: Finding Your Optimal Standing and Sitting Heights

A sit-stand desk provides maximum health and productivity benefits when customized to your specific posture and dimensions.

### 4 Steps to Find Your Optimal Standing Height:
1. **Wear everyday footwear:** Put on the shoes you typically wear while working at your desk (or stay in socks if you work barefoot; shoes often add 2 to 3 cm / ~1 inch).
2. **Adopt a relaxed, natural posture:** Stand upright in front of the desk, let your shoulders drop completely (do not hunch or tense upwards), and bend your elbows at roughly 90° to 100° (hands angled slightly downwards).
3. **Align table height with your elbows:** Raise the desk until the top edge of the tabletop sits level with or slightly below your forearms. When resting your hands on your keyboard or mouse, your wrists should remain neutral, without elbows pushed up or dangling unsupported.
4. **Position your monitor:** The top text/content line of your screen should sit at or slightly below eye level, keeping your head tilted slightly downward (approx. 15–20° angle).

### 4 Steps to Find Your Optimal Sitting Height:
1. **Adjust office chair and leg angle:** Sit fully back against the lumbar support. Your feet should rest flat on the floor. Your knee angle should be 90° to 100°, with roughly one hand's width between the edge of the seat pan and the back of your knees (ensuring free blood circulation).
2. **Maintain upright posture:** Keep your pelvis upright and rest against the chair backrest. Consciously drop and relax your shoulders.
3. **Match desk height to resting forearms:** Lower the desk so that your forearms rest horizontally on the desktop with your elbows bent at 90°. Your shoulders must not be pushed upwards. *(Golden rule: Always adjust the chair to your body first, then adjust the desk to the chair!)*
4. **Set monitor distance and height:** Position your monitor roughly one arm's length (50–70 cm) away. The top edge of the screen should align with eye level to keep your cervical spine completely relaxed.

---

## 🙏 Credits & Acknowledgments

This project builds upon the fantastic reverse-engineering work and documentation of the open-source community:

* **[ivanwick (Ivan Wick)](https://github.com/ivanwick/bekantfirmware):** Pioneer of the original IKEA Bekant LIN-bus reverse engineering, free firmware architecture, and hardware pinout documentation.
* **[DieSteinhose](https://github.com/DieSteinhose/bekantfirmware):** Creator of the 1-handed double-tap gesture control for the Bekant controller.
* **[GuentherMair](https://github.com/GuentherMair/bekantfirmware):** Upstream codebase maintenance and reverse-engineering documentation.
* **Microchip Technology:** For the official ICSP programming specification DS41397B for the PIC16(L)F193X family.
* **iFixit & Community Testers:** For detailed teardowns, schematic analysis, and hardware revisions insights.

---

## 📄 License

This project is licensed under the same open-source MIT license as the upstream codebase (see [firmware/LICENSE](firmware/LICENSE)).
Modifications for dynamic radiator protection, EEPROM limits, and ESP32/ESP8266 programmers are free for personal, educational, and open-source use and distribution.
