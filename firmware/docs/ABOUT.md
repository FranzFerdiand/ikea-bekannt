# Architecture & Technical Background

This firmware builds upon the open-source reverse-engineering of the IKEA BEKANT motorized desk controller:
- [ivanwick/bekantfirmware](https://github.com/ivanwick/bekantfirmware): Original position memory implementation and LIN-bus protocol discovery.
- [DieSteinhose/bekantfirmware](https://github.com/DieSteinhose/bekantfirmware): Double-tap 1-hand gesture fork.
- [GuentherMair/bekantfirmware](https://github.com/GuentherMair/bekantfirmware): Cleaned core architecture and reverse-engineering references.

## Key Features & Modifications

1. **Dual-Gesture Support (Unified State Machine)**:
   Supports both gesture styles simultaneously:
   - Ivanwick's combo-press: hold one button, tap the other.
   - DieSteinhose's double-tap: tap-tap within 500 ms.
   The unified state machine in `src/btn/btn.c` emits identical `INPUT_MEM_UP` / `INPUT_MEM_DOWN` events.

2. **Dynamic Radiator Collision Protection & Endstop Unlock**:
   The controller monitors desk height and blocks driving below a configured minimum height (`low_pos`), protecting obstacles like radiators beneath the desk.
   - A dedicated unlock gesture directly at the lower limit allows intentionally overriding and lowering the stop boundary when needed.

3. **8-Second Factory Reset**:
   Holding both buttons for 8 seconds clears EEPROM position memory back to pristine state (`0xFFFF`), resetting customized memory slots.

4. **OEM C Reconstruction & Endstop Analysis**:
   Includes an in-depth reverse-engineered C reconstruction in `oem_reconstruction/` for hardware documentation and reference.

## Module Overview

- **`src/btn/`**: Unified button state machine with stacked thresholds (3 s save, 8 s reset).
- **`src/bekant/bui.c`**: Higher-level UI handling user gestures, target heights, and EEPROM persistence.
- **`src/bekant/eeprom_defaults.{c,h}`**: Non-volatile storage management for position limits and defaults.
- **`src/bekant/bctrl.{c,h}`**: LIN-bus motor control plane issuing motion commands.
- **`src/bekant/endstop.{c,h}`**: Endstop / over-travel detection.
- **`src/lin/lin_d.{c,h}`**: Local Interconnect Network (LIN) driver communicating with the desk legs.
