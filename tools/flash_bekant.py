#!/usr/bin/env python3
"""
flash_bekant.py - Überträgt Intel-HEX auf den PIC16LF1938 via ESP32 ICSP Programmer
"""
import sys
import time
import serial

def parse_intel_hex(filename):
    words = {}
    ext_addr = 0
    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            if not line.startswith(':'):
                continue
            byte_count = int(line[1:3], 16)
            addr = int(line[3:7], 16)
            rec_type = int(line[7:9], 16)
            data = line[9:9 + byte_count * 2]

            if rec_type == 0x04:  # Extended Linear Address
                ext_addr = int(data, 16) << 16
            elif rec_type == 0x00:  # Data Record
                full_byte_addr = ext_addr + addr
                for i in range(0, byte_count, 2):
                    low_b = int(data[i*2:i*2+2], 16)
                    high_b = int(data[i*2+2:i*2+4], 16)
                    word_val = ((high_b << 8) | low_b) & 0x3FFF
                    word_addr = (full_byte_addr + i) // 2
                    # Nur Flash (0x0000-0x3FFF) und Config-Words (0x8007-0x8008)
                    if word_addr < 0x4000 or word_addr in (0x8007, 0x8008):
                        words[word_addr] = word_val
    return words

def main():
    if len(sys.argv) < 3:
        print("Nutzung: python flash_bekant.py <COM-Port> <pfad_zur_datei.hex>")
        print("Beispiel: python flash_bekant.py COM3 bekantfirmware.hex")
        sys.exit(1)

    port = sys.argv[1]
    hex_file = sys.argv[2]
    words = parse_intel_hex(hex_file)
    print(f"[*] {len(words)} 14-Bit-Wörter aus HEX-Datei geladen.")

    ser = serial.Serial(port, 115200, timeout=2)
    time.sleep(2)

    ser.write(b"ID\n")
    resp = ser.readline().decode().strip()
    print(f"[*] Chip-Antwort: {resp}")
    if "0x0000" in resp or "0x3FFF" in resp or "ID:" not in resp:
        print("[!] Fehler: PIC16LF1938 nicht erkannt! Bitte Verkabelung prüfen.")
        sys.exit(1)

    print("[*] Lösche Chip (Bulk Erase)...")
    ser.write(b"ERASE\n")
    erase_resp = ser.readline().decode().strip()
    print(f"    -> {erase_resp}")

    total = len(words)
    for idx, (w_addr, w_val) in enumerate(sorted(words.items()), 1):
        if w_val == 0x3FFF:
            continue  # Leere Wörter überspringen
        cmd = f"WPROG:{w_addr:04X}:{w_val:04X}\n"
        ser.write(cmd.encode())
        ack = ser.readline().decode().strip()
        if ack != "OK":
            print(f"\n[!] Schreibfehler bei Adresse 0x{w_addr:04X}: {ack}")
            sys.exit(1)
        sys.stdout.write(f"\r[*] Flashe: {idx}/{total} (Adresse 0x{w_addr:04X})")
        sys.stdout.flush()

    print("\n[+] Flashen erfolgreich abgeschlossen!")
    ser.close()

if __name__ == "__main__":
    main()
