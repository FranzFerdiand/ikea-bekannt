/*
 * ESP8266 ICSP Programmer für Microchip PIC16LF1938 (IKEA Bekant)
 * Exakt nach Microchip DS41397B Spezifikation:
 * - 32-Wort-Row-Programming für Program Flash (0x0000..0x3FFF)
 * - Einzelwort-Programmierung für Configuration Words (0x8007, 0x8008)
 * - 3.3V Low-Voltage Programming (LVP) via Pin D5 (GPIO 14)
 */
#include <Arduino.h>

#define PIN_MCLR  14  // Pin D5 (MCLR / VPP)
#define PIN_PGD   12  // Pin D6 (PGD / ICSPDAT)
#define PIN_PGC   13  // Pin D7 (PGC / ICSPCLK)

// ICSP Kommandos laut Microchip DS41397B
#define CMD_LOAD_CONFIG         0x00  // Setzt PC auf 0x8000 (erfordert 16-Bit Dummy)
#define CMD_LOAD_DATA_PROG      0x02  // Lädt 14-Bit Latch-Wort
#define CMD_READ_DATA_PROG      0x04  // Liest 14-Bit Wort
#define CMD_INC_ADDRESS         0x06  // Erhöht PC um 1
#define CMD_BEGIN_INT_TIMED_PRG 0x08  // Brennt Zeile / Wort
#define CMD_BULK_ERASE_PROG     0x09  // Löscht Flash & Config
#define CMD_BULK_ERASE_DATA     0x0B  // Löscht Data EEPROM
#define CMD_RESET_ADDRESS       0x16  // Setzt PC zurück auf 0x0000

// 1. Sendet 6 Kommando-Bits (LSB first, Übernahme bei fallender Flanke)
void icsp_send_cmd(uint8_t cmd) {
    pinMode(PIN_PGD, OUTPUT);
    for (int i = 0; i < 6; i++) {
        digitalWrite(PIN_PGD, (cmd >> i) & 0x01);
        digitalWrite(PIN_PGC, HIGH);
        delayMicroseconds(2);
        digitalWrite(PIN_PGC, LOW);
        delayMicroseconds(2);
    }
    digitalWrite(PIN_PGD, LOW);
    delayMicroseconds(3);
}

// 2. Sendet 16 Daten-Bits: Start(0) + 14 Bit Daten (LSB first) + Stop(0)
void icsp_send_data(uint16_t data14) {
    uint16_t frame = (data14 & 0x3FFF) << 1;
    pinMode(PIN_PGD, OUTPUT);
    for (int i = 0; i < 16; i++) {
        digitalWrite(PIN_PGD, (frame >> i) & 0x01);
        digitalWrite(PIN_PGC, HIGH);
        delayMicroseconds(2);
        digitalWrite(PIN_PGC, LOW);
        delayMicroseconds(2);
    }
    digitalWrite(PIN_PGD, LOW);
    delayMicroseconds(3);
}

// 3. Liest 16 Daten-Bits: Start(0) + 14 Bit Daten (LSB first) + Stop(0)
uint16_t icsp_read_data() {
    pinMode(PIN_PGD, INPUT);
    delayMicroseconds(2);

    uint16_t data = 0;
    for (int i = 0; i < 16; i++) {
        digitalWrite(PIN_PGC, HIGH);
        delayMicroseconds(2);
        uint8_t bit_val = digitalRead(PIN_PGD);
        digitalWrite(PIN_PGC, LOW);
        delayMicroseconds(2);

        if (i >= 1 && i <= 14) {
            if (bit_val) {
                data |= (1 << (i - 1));
            }
        }
    }
    delayMicroseconds(3);
    return data & 0x3FFF;
}

void enterProgMode() {
    pinMode(PIN_MCLR, OUTPUT);
    pinMode(PIN_PGC, OUTPUT);
    pinMode(PIN_PGD, OUTPUT);

    digitalWrite(PIN_PGC, LOW);
    digitalWrite(PIN_PGD, LOW);

    // MCLR High -> Low Pulse
    digitalWrite(PIN_MCLR, HIGH);
    delay(10);
    digitalWrite(PIN_MCLR, LOW);
    delayMicroseconds(500);

    // 32-Bit LVP Key "MCHP" (0x4D434850) + 1 Extratakt
    uint32_t key = 0x4D434850;
    for (int i = 0; i < 32; i++) {
        digitalWrite(PIN_PGD, (key >> i) & 0x01);
        digitalWrite(PIN_PGC, HIGH);
        delayMicroseconds(2);
        digitalWrite(PIN_PGC, LOW);
        delayMicroseconds(2);
    }
    digitalWrite(PIN_PGD, LOW);
    digitalWrite(PIN_PGC, HIGH);
    delayMicroseconds(2);
    digitalWrite(PIN_PGC, LOW);
    delay(5);
}

void exitProgMode() {
    pinMode(PIN_PGD, INPUT);
    pinMode(PIN_PGC, INPUT);
    pinMode(PIN_MCLR, OUTPUT);
    digitalWrite(PIN_MCLR, HIGH);
}

// Setzt den PC sauber auf eine beliebige Adresse
void icsp_goto_address(uint16_t addr) {
    if (addr >= 0x8000) {
        icsp_send_cmd(CMD_LOAD_CONFIG);
        icsp_send_data(0x0000); // Dummy
        uint16_t steps = addr - 0x8000;
        for (uint16_t i = 0; i < steps; i++) {
            icsp_send_cmd(CMD_INC_ADDRESS);
        }
    } else {
        icsp_send_cmd(CMD_RESET_ADDRESS);
        for (uint16_t i = 0; i < addr; i++) {
            icsp_send_cmd(CMD_INC_ADDRESS);
        }
    }
}

uint16_t readDeviceID() {
    enterProgMode();
    icsp_goto_address(0x8006);
    icsp_send_cmd(CMD_READ_DATA_PROG);
    return icsp_read_data();
}

void bulkErase() {
    enterProgMode();
    icsp_send_cmd(CMD_LOAD_CONFIG);
    icsp_send_data(0x3FFF);
    icsp_send_cmd(CMD_BULK_ERASE_PROG);
    delay(15);
    icsp_send_cmd(CMD_BULK_ERASE_DATA);
    delay(15);
    exitProgMode();
}

// Liest ein Wort an beliebiger Adresse
uint16_t readWordAt(uint16_t addr) {
    enterProgMode();
    icsp_goto_address(addr);
    icsp_send_cmd(CMD_READ_DATA_PROG);
    return icsp_read_data();
}

// Schreibt ein einzelnes Config-Wort (0x8007, 0x8008)
void writeConfigWord(uint16_t addr, uint16_t val) {
    enterProgMode();
    icsp_goto_address(addr);
    icsp_send_cmd(CMD_LOAD_DATA_PROG);
    icsp_send_data(val);
    icsp_send_cmd(CMD_BEGIN_INT_TIMED_PRG);
    delay(10); // Config Words brauchen bis zu 10ms
}

// Schreibt eine komplette 32-Wort-Row in den Programmspeicher
void writeRow32(uint16_t start_addr, const uint16_t *words) {
    enterProgMode();
    icsp_goto_address(start_addr);

    // Lade die ersten 31 Wörter (Latches 0 bis 30)
    for (int i = 0; i < 31; i++) {
        icsp_send_cmd(CMD_LOAD_DATA_PROG);
        icsp_send_data(words[i]);
        icsp_send_cmd(CMD_INC_ADDRESS);
    }

    // 32. Wort laden (Latch 31) und Brennen auslösen
    icsp_send_cmd(CMD_LOAD_DATA_PROG);
    icsp_send_data(words[31]);
    icsp_send_cmd(CMD_BEGIN_INT_TIMED_PRG);
    delay(4); // T_PROG für Flash-Zeile
}

void setup() {
    Serial.begin(115200);
    pinMode(PIN_MCLR, OUTPUT);
    digitalWrite(PIN_MCLR, HIGH);
    pinMode(PIN_PGC, OUTPUT);
    pinMode(PIN_PGD, OUTPUT);
    digitalWrite(PIN_PGC, LOW);
    digitalWrite(PIN_PGD, LOW);
}

void loop() {
    yield();

    if (!Serial.available()) return;
    String line = Serial.readStringUntil('\n');
    line.trim();

    if (line == "ID") {
        uint16_t id = readDeviceID();
        Serial.printf("OK ID:0x%04X\n", id);
    }
    else if (line == "ERASE") {
        bulkErase();
        Serial.println("OK ERASED");
    }
    else if (line.startsWith("RPROG:")) {
        uint16_t addr = strtol(line.substring(6).c_str(), NULL, 16);
        uint16_t val = readWordAt(addr);
        Serial.printf("OK RPROG:0x%04X:0x%04X\n", addr, val);
    }
    else if (line.startsWith("WCONFIG:")) {
        // Format: WCONFIG:<ADDR>:<VAL>
        int p1 = line.indexOf(':');
        int p2 = line.indexOf(':', p1 + 1);
        uint16_t addr = strtol(line.substring(p1 + 1, p2).c_str(), NULL, 16);
        uint16_t val  = strtol(line.substring(p2 + 1).c_str(), NULL, 16);
        writeConfigWord(addr, val);
        Serial.println("OK");
    }
    else if (line.startsWith("WROW32:")) {
        // Format: WROW32:<START_ADDR>:<W0>:<W1>:...:<W31>
        int cur_pos = 7; // Länge von "WROW32:"
        int next_col = line.indexOf(':', cur_pos);
        uint16_t start_addr = strtol(line.substring(cur_pos, next_col).c_str(), NULL, 16);
        cur_pos = next_col + 1;

        uint16_t row_words[32];
        for (int i = 0; i < 32; i++) {
            next_col = line.indexOf(':', cur_pos);
            if (next_col == -1) next_col = line.length();
            row_words[i] = strtol(line.substring(cur_pos, next_col).c_str(), NULL, 16);
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
