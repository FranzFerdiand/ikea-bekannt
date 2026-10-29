#!/usr/bin/env python3
"""
flash_bekant.py - Überträgt Intel-HEX auf den PIC16LF1938 via ESP8266/ESP32 ICSP Programmer
Verwendet 32-Wort-Row-Flashing (WROW32) und WCONFIG.
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

def send_and_expect(ser, cmd, expected="OK", retries=2, timeout_retry=0.1):
    for attempt in range(retries):
        ser.write((cmd + '\n').encode('utf-8'))
        resp = ser.readline().decode('utf-8', errors='replace').strip()
        if expected in resp:
            return resp
        time.sleep(timeout_retry)
    return resp

def read_word(ser, addr):
    ser.write(f"RPROG:{addr:04X}\n".encode('utf-8'))
    resp = ser.readline().decode('utf-8', errors='replace').strip()
    # Format: OK RPROG:0x8007:0x0FFC
    parts = resp.split(':')
    if len(parts) >= 3:
        try:
            return int(parts[2], 16)
        except ValueError:
            pass
    return None

def main():
    port = sys.argv[1] if len(sys.argv) > 1 else 'COM12'
    hex_file = sys.argv[2] if len(sys.argv) > 2 else 'bin/bekantfirmware.hex'
    
    words = parse_intel_hex(hex_file)
    prog_words = {k: v for k, v in words.items() if k < 0x4000}
    config_words = {k: v for k, v in words.items() if k in (0x8007, 0x8008)}
    
    print(f"[*] HEX-Datei: {hex_file}")
    print(f"[*] Flash-Wörter: {len(prog_words)} (0x0000..0x{max(prog_words):04X})")
    print(f"[*] Config-Wörter: {', '.join(f'0x{k:04X}=0x{v:04X}' for k, v in sorted(config_words.items()))}")

    ser = serial.Serial(port, 115200, timeout=2.0)
    time.sleep(0.8)
    ser.reset_input_buffer()

    print("[*] Prüfe Verbindung zum PIC16LF1938 via 3.3V LVP...")
    valid = False
    start_wait = time.time()
    last_status = ""
    while time.time() - start_wait < 15:
        ser.write(b"ID\n")
        resp = ser.readline().decode('utf-8', errors='replace').strip()
        if "OK ID:0x" in resp and "0x0000" not in resp and "0x3FFF" not in resp:
            print(f"[+] KONTAKT DA: {resp}")
            valid = True
            break
        if resp and resp != last_status:
            print(f"    Status: {resp}")
            last_status = resp
        time.sleep(0.1)

    if not valid:
        print("[!] Timeout beim Verbindungsaufbau zum PIC.")
        ser.close()
        sys.exit(1)

    # 1. Bulk Erase
    print("[*] LÖSCHE CHIP (Bulk Erase)...")
    ser.write(b"ERASE\n")
    erase_ack = ser.readline().decode('utf-8', errors='replace').strip()
    print("    ->", erase_ack)
    if "OK" not in erase_ack:
        print("[!] Fehler beim Erase:", erase_ack)
        ser.close()
        sys.exit(1)

    # 2. Programm-Flash zeilenweise (32 Wörter pro Zeile) schreiben
    max_addr = max(prog_words.keys())
    num_rows = (max_addr // 32) + 1
    print(f"[*] Schreibe {num_rows} Zeilen ({num_rows * 32} Wörter) mit WROW32...")

    t_start = time.time()
    written_rows = 0
    for r in range(num_rows):
        start_addr = r * 32
        row_words = [words.get(start_addr + i, 0x3FFF) for i in range(32)]
        
        # Falls die gesamte Zeile 0x3FFF ist, kann man sie nach einem Erase überspringen
        if all(w == 0x3FFF for w in row_words):
            continue

        words_hex = ":".join(f"{w:04X}" for w in row_words)
        cmd = f"WROW32:{start_addr:04X}:{words_hex}"
        ack = send_and_expect(ser, cmd, expected="OK")
        if "OK" not in ack:
            print(f"\n[!] Schreibfehler bei Zeile 0x{start_addr:04X}: {ack}")
            ser.close()
            sys.exit(1)

        written_rows += 1
        pct = int(((r + 1) / num_rows) * 100)
        sys.stdout.write(f"\r[*] Fortschritt Flash: {pct}% (Zeile {r + 1}/{num_rows})")
        sys.stdout.flush()

    # 3. Configuration Words schreiben
    print("\n[*] Schreibe Configuration Words...")
    for c_addr, c_val in sorted(config_words.items()):
        cmd = f"WCONFIG:{c_addr:04X}:{c_val:04X}"
        ack = send_and_expect(ser, cmd, expected="OK")
        print(f"    WCONFIG:0x{c_addr:04X}:0x{c_val:04X} -> {ack}")

    # 4. Verifikation wichtiger Adressen
    print("[*] Verifiziere geschriebene Daten...")
    read_8007 = read_word(ser, 0x8007)
    read_8008 = read_word(ser, 0x8008)
    read_0000 = read_word(ser, 0x0000)
    read_0001 = read_word(ser, 0x0001)

    print(f"    0x8007 (CONFIG1): Soll=0x{config_words.get(0x8007, 0):04X}, Ist={f'0x{read_8007:04X}' if read_8007 is not None else 'FEHLER'}")
    print(f"    0x8008 (CONFIG2): Soll=0x{config_words.get(0x8008, 0):04X}, Ist={f'0x{read_8008:04X}' if read_8008 is not None else 'FEHLER'}")
    print(f"    0x0000 (RESET)  : Soll=0x{prog_words.get(0, 0):04X}, Ist={f'0x{read_0000:04X}' if read_0000 is not None else 'FEHLER'}")
    print(f"    0x0001          : Soll=0x{prog_words.get(1, 0):04X}, Ist={f'0x{read_0001:04X}' if read_0001 is not None else 'FEHLER'}")

    ser.write(b"DONE\n")
    done_ack = ser.readline().decode('utf-8', errors='replace').strip()
    print("    ->", done_ack)

    duration = time.time() - t_start
    print(f"\n[+] FLASH-VORGANG ERFOLGREICH in {duration:.1f}s beendet!")

    if read_8007 == config_words.get(0x8007) and read_0000 == prog_words.get(0):
        print("[+] VERIFIKATION BESTÄTIGT: Alle Schlüsseldaten stimmen exakt überein!")
    else:
        print("[!] WARNUNG: Verifikationsabweichung festgestellt. Bitte Werte prüfen.")

    ser.close()

if __name__ == "__main__":
    main()
