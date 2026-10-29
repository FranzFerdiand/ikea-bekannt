#!/usr/bin/env python3
"""
check_diag.py - Schnelle Diagnose für PIC16LF1938 via ESP32/ESP8266 ICSP Programmer
Liest Device ID, Configuration Words (0x8007, 0x8008) und den Programmstart (0x0000..0x0004) aus.
"""
import sys
import time
import serial

def main():
    port = sys.argv[1] if len(sys.argv) > 1 else 'COM12'
    print(f"[*] Öffne {port} (115200 Baud)...")

    try:
        ser = serial.Serial(port, 115200, timeout=1.5)
    except Exception as e:
        print(f"[!] Fehler beim Öffnen von {port}: {e}")
        sys.exit(1)

    time.sleep(1.0)
    ser.reset_input_buffer()

    commands = [
        ('ID',          'Device ID & Revision'),
        ('RPROG:8006',  'Device ID Register'),
        ('RPROG:8007',  'CONFIG1 (FOSC / WDT / MCLR)'),
        ('RPROG:8008',  'CONFIG2 (LVP / WRT / BOR)'),
        ('RPROG:0000',  'Flash 0x0000 (Reset-Vektor)'),
        ('RPROG:0001',  'Flash 0x0001 (Goto Hauptprogramm)'),
        ('RPROG:0002',  'Flash 0x0002'),
        ('RPROG:0003',  'Flash 0x0003'),
        ('RPROG:0004',  'Flash 0x0004 (Interrupt-Vektor)'),
    ]

    print("\n--- PIC16LF1938 Speicher- und Status-Diagnose ---")
    for cmd, desc in commands:
        ser.write((cmd + '\n').encode('utf-8'))
        resp = ser.readline().decode('utf-8', errors='replace').strip()
        print(f"  {cmd:12} -> {resp:<24} ({desc})")

    ser.write(b"DONE\n")
    ser.readline()
    ser.close()
    print("--------------------------------------------------\n")

if __name__ == "__main__":
    main()
