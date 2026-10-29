#!/usr/bin/env python3
"""
flash_bekant.py - Überträgt Intel-HEX auf den PIC16LF1938 via ESP8266/ESP32 ICSP Programmer
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

            if rec_type == 0x04:
                ext_addr = int(data, 16) << 16
            elif rec_type == 0x00:
                full_byte_addr = ext_addr + addr
                for i in range(0, byte_count, 2):
                    low_b = int(data[i*2:i*2+2], 16)
                    high_b = int(data[i*2+2:i*2+4], 16)
                    word_val = ((high_b << 8) | low_b) & 0x3FFF
                    word_addr = (full_byte_addr + i) // 2
                    if word_addr < 0x4000 or word_addr in (0x8007, 0x8008):
                        words[word_addr] = word_val
    return words

def main():
    port = sys.argv[1] if len(sys.argv) > 1 else 'COM12'
    hex_file = sys.argv[2] if len(sys.argv) > 2 else 'bin/bekantfirmware.hex'
    words = parse_intel_hex(hex_file)
    print(f"[*] {len(words)} Wörter geladen aus {hex_file}.")

    ser = serial.Serial(port, 115200, timeout=1.5)
    time.sleep(0.5)

    print("[*] BEREIT! Halte jetzt die 9V an Pin 1 (MCLR). Wartezeit bis zu 90s...")
    valid = False
    start_wait = time.time()
    last_status = ""
    while time.time() - start_wait < 90:
        ser.write(b"ID\n")
        resp = ser.readline().decode().strip()
        if "OK ID:0x" in resp and "0x0000" not in resp and "0x3FFF" not in resp:
            print(f"\n[+] KONTAKT DA! Erkannt: {resp}!")
            valid = True
            break
        if resp and resp != last_status:
            sys.stdout.write(f"\n[{time.strftime('%H:%M:%S')}] Status: {resp} ")
            last_status = resp
        else:
            sys.stdout.write(".")
        sys.stdout.flush()
        time.sleep(0.06)

    if not valid:
        print("\n[!] Timeout (90s abgelaufen).")
        ser.close()
        sys.exit(1)

    print("\n[*] LÖSCHE CHIP (Bulk Erase)...")
    ser.write(b"ERASE\n")
    erase_ack = ser.readline().decode().strip()
    print("    ->", erase_ack)

    total = len(words)
    print(f"[*] FLASHE {total} WÖRTER (High-Speed ca. 12-15s)... BITTE 9V WEITER FESTHALTEN!")
    t_start = time.time()
    for idx, (w_addr, w_val) in enumerate(sorted(words.items()), 1):
        if w_val == 0x3FFF:
            continue
        cmd = f"WPROG:{w_addr:04X}:{w_val:04X}\n"
        ser.write(cmd.encode())
        ack = ser.readline().decode().strip()
        if ack != "OK":
            # Retry once
            time.sleep(0.01)
            ser.write(cmd.encode())
            ack = ser.readline().decode().strip()
            if ack != "OK":
                print(f"\n[!] Schreibfehler bei 0x{w_addr:04X}: {ack}")
                ser.close()
                sys.exit(1)
        if idx % 100 == 0 or idx == total:
            pct = int((idx / total) * 100)
            sys.stdout.write(f"\r[*] Fortschritt: {pct}% ({idx}/{total} Wörter)")
            sys.stdout.flush()

    ser.write(b"DONE\n")
    ser.readline()

    duration = time.time() - t_start
    print(f"\n\n[+] ERFOLGREICH GEFLASHED in {duration:.1f}s!")
    print("[+] DU KANNST DAS 9V-KABEL JETZT ABNEHMEN!")
    ser.close()

if __name__ == "__main__":
    main()
