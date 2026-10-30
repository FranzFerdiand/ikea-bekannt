# IKEA Bekant Firmware Source & Technical Documentation

This directory contains the C source code and technical reverse-engineering documentation for the IKEA Bekant custom firmware.

For the main project documentation, wiring diagrams, and ESP32/ESP8266 flashing guide, please see the top-level [README.md](../README.md) (or [English README.en.md](../README.en.md)).

## Directory Structure

* **`src/`**: Primary C firmware source code:
  * `main.c` / `system.c` / `user.c`: Microcontroller startup, clock setup, and peripheral initialization.
  * `interrupts.c`: ISR handling Timer2, Timer4, and UART interrupts.
  * `configuration_bits.c`: PIC16LF1938 fuses (`LVP = ON`, `FOSC = INTOSC`).
  * `btn/`: Dual-gesture button state machine (combo-press and double-tap).
  * `bekant/`: Higher-level application logic (`bui.c`), motor control plane (`bctrl.c`), EEPROM persistence (`eeprom_defaults.c`), and radiator limits (`radiator_config.h`).
  * `lin/`: Local Interconnect Network (LIN) bus protocol implementation.
* **`docs/`**: Reverse-engineering and architectural deep dives:
  * [`ABOUT.md`](docs/ABOUT.md): Architecture overview and key technical improvements.
  * [`COMPARISON.md`](docs/COMPARISON.md): Analysis of upstream forks (`ivanwick` vs `DieSteinhose`).
  * [`ENDSTOP_ANALYSIS.md`](docs/ENDSTOP_ANALYSIS.md): Root-cause analysis of physical endstop detection.
  * [`TESTING.md`](docs/TESTING.md): Timing and test scenarios for the button state machine.
  * [`TRACE.md`](docs/TRACE.md): State transition traces.
* **`oem_reconstruction/`**: Complete C reconstruction of the original stock IKEA firmware for reference.
* **`INSTALL.md`**: Legacy guide for flashing with PICkit 3 programmers via MPLAB X / pk2cmd (for users who prefer hardware programmers over ESP32/ESP8266).
* **`Makefile`**: Build script for building with Microchip XC8 compiler (`xc8-cc`).
* **`LICENSE`**: MIT open-source license and upstream acknowledgments.
