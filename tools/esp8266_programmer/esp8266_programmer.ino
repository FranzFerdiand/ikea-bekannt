/*
 * ESP8266 ICSP Programmer für Microchip PIC16LF1938 (IKEA Bekant)
 * Korrigierte Version:
 * - Kein erneutes enterProgMode() vor jeder Zeile (verhindert Bit-Shift & 0x0008-Bug!)
 * - Smartes PC-Tracking (kein langsames Hochzählen ab 0x0000 bei jeder Zeile)
 * - Sauberes 3.3V LVP nach Microchip DS41397B
 */
#include <Arduino.h>

#define PIN_MCLR  14  // Pin D5 (GPIO 14 -> MCLR / VPP)
#define PIN_PGD   12  // Pin D6 (GPIO 12 -> PGD / ICSPDAT)
#define PIN_PGC   13  // Pin D7 (GPIO 13 -> PGC / ICSPCLK)

// ICSP Kommandos laut Microchip DS41397B
#define CMD_LOAD_CONFIG         0x00  // Setzt PC auf 0x8000 (erfordert 16-Bit Dummy)
#define CMD_LOAD_DATA_PROG      0x02  // Lädt 14-Bit Latch-Wort
#define CMD_READ_DATA_PROG      0x04  // Liest 14-Bit Wort
#define CMD_INC_ADDRESS         0x06  // Erhöht PC um 1
#define CMD_BEGIN_INT_TIMED_PRG 0x08  // Brennt Zeile / Wort
#define CMD_BULK_ERASE_PROG     0x09  // Löscht Flash & Config
#define CMD_BULK_ERASE_DATA     0x0B  // Löscht Data EEPROM
#define CMD_RESET_ADDRESS       0x16  // Setzt PC zurück auf 0x0000

static bool in_prog_mode = false;
static uint16_t current_pc = 0xFFFF; // Aktuelle Position des PIC-Programmzählers

// 1. Sendet 6 Kommando-Bits (LSB first, Übernahme bei fallender Flanke)
void icsp_send_cmd(uint8_t cmd) {
    pinMode(PIN_PGD, OUTPUT);
    for (int i = 0; i < 6; i++) {
        digitalWrite(PIN_PGD, (cmd >> i) & 0x01);
        digitalWrite(PIN_PGC, HIGH);
        delayMicroseconds(3);
        digitalWrite(PIN_PGC, LOW);
        delayMicroseconds(3);
    }
    digitalWrite(PIN_PGD, LOW);
    delayMicroseconds(5); // Tdly1 (min 1 µs)
}

// 2. Sendet 16 Daten-Bits: Start(0) + 14 Bit Daten (LSB first) + Stop(0)
void icsp_send_data(uint16_t data14) {
    uint16_t frame = (data14 & 0x3FFF) << 1;
    pinMode(PIN_PGD, OUTPUT);
    for (int i = 0; i < 16; i++) {
        digitalWrite(PIN_PGD, (frame >> i) & 0x01);
        digitalWrite(PIN_PGC, HIGH);
        delayMicroseconds(3);
        digitalWrite(PIN_PGC, LOW);
        delayMicroseconds(3);
    }
    digitalWrite(PIN_PGD, LOW);
    delayMicroseconds(5);
}

// 3. Liest 16 Daten-Bits: Start(0) + 14 Bit Daten (LSB first) + Stop(0)
uint16_t icsp_read_data() {
    pinMode(PIN_PGD, INPUT);
    delayMicroseconds(3);

    uint16_t data = 0;
    for (int i = 0; i < 16; i++) {
        digitalWrite(PIN_PGC, HIGH);
        delayMicroseconds(3);
        uint8_t bit_val = digitalRead(PIN_PGD);
        digitalWrite(PIN_PGC, LOW);
        delayMicroseconds(3);

        if (i >= 1 && i <= 14) {
            if (bit_val) {
                data |= (1 << (i - 1));
            }
        }
    }
    pinMode(PIN_PGD, OUTPUT);
    digitalWrite(PIN_PGD, LOW);
    delayMicroseconds(5);
    return data & 0x3FFF;
}

// Betritt den 3.3V LVP-Modus NUR DANN, wenn wir noch nicht drin sind!
void ensureProgMode() {
    if (in_prog_mode) return;

    pinMode(PIN_MCLR, OUTPUT);
    pinMode(PIN_PGC, OUTPUT);
    pinMode(PIN_PGD, OUTPUT);

    digitalWrite(PIN_PGC, LOW);
    digitalWrite(PIN_PGD, LOW);

    // Sauberer Reset-Puls auf MCLR
    digitalWrite(PIN_MCLR, HIGH);
    delay(20);
    digitalWrite(PIN_MCLR, LOW);
    delay(2); // Tents / Tenth (min 250 µs)

    // 32-Bit LVP Key "MCHP" (0x4D434850) LSB first + 1 Extratakt
    uint32_t key = 0x4D434850UL;
    for (int i = 0; i < 32; i++) {
        digitalWrite(PIN_PGD, (key >> i) & 0x01);
        digitalWrite(PIN_PGC, HIGH);
        delayMicroseconds(3);
        digitalWrite(PIN_PGC, LOW);
        delayMicroseconds(3);
    }
    digitalWrite(PIN_PGD, LOW);
    digitalWrite(PIN_PGC, HIGH);
    delayMicroseconds(3);
    digitalWrite(PIN_PGC, LOW);
    delay(5);

    in_prog_mode = true;
    current_pc = 0x0000;
}

void exitProgMode() {
    pinMode(PIN_PGD, INPUT);
    pinMode(PIN_PGC, INPUT);
    pinMode(PIN_MCLR, OUTPUT);
    digitalWrite(PIN_MCLR, HIGH);
    in_prog_mode = false;
    current_pc = 0xFFFF;
}

// Setzt den PC intelligent und schnell auf jede Adresse
void icsp_goto_address(uint16_t addr) {
    ensureProgMode();

    if (addr >= 0x8000) {
        // Wechsel in den Configuration-Bereich (0x8000..0x800F)
        if (current_pc < 0x8000 || current_pc > addr) {
            icsp_send_cmd(CMD_LOAD_CONFIG);
            icsp_send_data(0x0000); // Dummy-Wort setzt PC auf 0x8000
            current_pc = 0x8000;
        }
        while (current_pc < addr) {
            icsp_send_cmd(CMD_INC_ADDRESS);
            current_pc++;
        }
    } else {
        // Im normalen Programmspeicher (0x0000..0x3FFF)
        if (current_pc >= 0x8000 || current_pc > addr) {
            icsp_send_cmd(CMD_RESET_ADDRESS); // Setzt PC direkt auf 0x0000
            current_pc = 0x0000;
        }
        while (current_pc < addr) {
            icsp_send_cmd(CMD_INC_ADDRESS);
            current_pc++;
            if ((current_pc & 0x01FF) == 0) yield(); // Verhindert ESP8266 WDT-Reset
        }
    }
}

uint16_t readDeviceID() {
    icsp_goto_address(0x8006);
    icsp_send_cmd(CMD_READ_DATA_PROG);
    return icsp_read_data();
}

void bulkErase() {
    // Für Bulk Erase einmal frisch in den ProgMode gehen
    in_prog_mode = false;
    ensureProgMode();
    // Laut DS41397B: Load Config (0x00) damit Config Words + Flash gelöscht werden!
    icsp_send_cmd(CMD_LOAD_CONFIG);
    icsp_send_data(0x3FFF);
    icsp_send_cmd(CMD_BULK_ERASE_PROG);
    delay(15); // Tera (max 10 ms)
    icsp_send_cmd(CMD_BULK_ERASE_DATA);
    delay(15);
    // Nach dem Erase sauber neu synchronisieren
    exitProgMode();
    delay(10);
    ensureProgMode();
}

uint16_t readWordAt(uint16_t addr) {
    icsp_goto_address(addr);
    icsp_send_cmd(CMD_READ_DATA_PROG);
    return icsp_read_data();
}

void writeConfigWord(uint16_t addr, uint16_t val) {
    icsp_goto_address(addr);
    icsp_send_cmd(CMD_LOAD_DATA_PROG);
    icsp_send_data(val);
    icsp_send_cmd(CMD_BEGIN_INT_TIMED_PRG);
    delay(10); // Tprog für Config Words (5 ms laut Datenblatt)
}

void writeRow32(uint16_t start_addr, const uint16_t *words) {
    icsp_goto_address(start_addr);

    for (int i = 0; i < 31; i++) {
        icsp_send_cmd(CMD_LOAD_DATA_PROG);
        icsp_send_data(words[i]);
        icsp_send_cmd(CMD_INC_ADDRESS);
        current_pc++;
    }

    icsp_send_cmd(CMD_LOAD_DATA_PROG);
    icsp_send_data(words[31]);
    icsp_send_cmd(CMD_BEGIN_INT_TIMED_PRG);
    delay(5); // Tprog für 32-Wort Flash-Row (max 2.5 ms)

    // Nächste Zeile vorbereitet
    icsp_send_cmd(CMD_INC_ADDRESS);
    current_pc++;
}

void setup() {
    Serial.begin(115200);
    exitProgMode();
}

void loop() {
    yield();

    if (!Serial.available()) return;
    String line = Serial.readStringUntil('\n');
    line.trim();

    if (line == "ID") {
        // Erzwingt frischen Eintritt bei ID-Abfrage
        in_prog_mode = false;
        uint16_t id = readDeviceID();
        Serial.printf("OK ID:0x%04X\n", id);
    }
    else if (line == "ERASE") {
        bulkErase();
        Serial.println("OK ERASED");
    }
    else if (line.startsWith("RPROG:")) {
        uint16_t addr = (uint16_t)strtoul(line.substring(6).c_str(), NULL, 16);
        uint16_t val = readWordAt(addr);
        Serial.printf("OK RPROG:0x%04X:0x%04X\n", addr, val);
    }
    else if (line.startsWith("WCONFIG:")) {
        int p1 = line.indexOf(':');
        int p2 = line.indexOf(':', p1 + 1);
        uint16_t addr = (uint16_t)strtoul(line.substring(p1 + 1, p2).c_str(), NULL, 16);
        uint16_t val  = (uint16_t)strtoul(line.substring(p2 + 1).c_str(), NULL, 16);
        writeConfigWord(addr, val);
        Serial.println("OK");
    }
    else if (line.startsWith("WROW32:")) {
        int cur_pos = 7;
        int next_col = line.indexOf(':', cur_pos);
        uint16_t start_addr = (uint16_t)strtoul(line.substring(cur_pos, next_col).c_str(), NULL, 16);
        cur_pos = next_col + 1;

        uint16_t row_words[32];
        for (int i = 0; i < 32; i++) {
            next_col = line.indexOf(':', cur_pos);
            if (next_col == -1) next_col = line.length();
            row_words[i] = (uint16_t)strtoul(line.substring(cur_pos, next_col).c_str(), NULL, 16);
            cur_pos = next_col + 1;
        }

        writeRow32(start_addr, row_words);
        Serial.println("OK");
    }
    else if (line == "DONE") {
        exitProgMode();
        Serial.println("OK DONE");
    }
}
