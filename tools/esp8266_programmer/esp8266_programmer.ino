/*
 * ESP8266 ICSP Programmer für Microchip PIC16LF1938 (IKEA Bekant)
 * Optimiert für schnelles sequentielles Schreiben (High-Voltage 9V & LVP).
 */
#include <Arduino.h>

#define PIN_MCLR  14  // Pin D5 (bleibt bei 9V frei)
#define PIN_PGD   12  // Pin D6
#define PIN_PGC   13  // Pin D7

// ICSP Kommandos laut DS41397
#define CMD_LOAD_CONFIG         0x00
#define CMD_LOAD_DATA_PROG      0x02
#define CMD_LOAD_DATA_DATA      0x03
#define CMD_READ_DATA_PROG      0x04
#define CMD_READ_DATA_DATA      0x05
#define CMD_INC_ADDRESS         0x06
#define CMD_RESET_ADDRESS       0x16
#define CMD_BEGIN_INT_TIMED_PRG 0x08
#define CMD_BULK_ERASE_PROG     0x09
#define CMD_BULK_ERASE_DATA     0x0B

int32_t current_pc = -1;

void sendBit(uint8_t b) {
    digitalWrite(PIN_PGC, HIGH);
    digitalWrite(PIN_PGD, b ? HIGH : LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_PGC, LOW);
    delayMicroseconds(2);
}

uint8_t readBit() {
    uint8_t b = 0;
    digitalWrite(PIN_PGC, HIGH);
    delayMicroseconds(2);
    b = digitalRead(PIN_PGD);
    digitalWrite(PIN_PGC, LOW);
    delayMicroseconds(2);
    return b;
}

void sendCmd(uint8_t cmd) {
    pinMode(PIN_PGD, OUTPUT);
    for (int i = 0; i < 6; i++) {
        sendBit((cmd >> i) & 0x01);
    }
    delayMicroseconds(3);
}

void sendData14(uint16_t data) {
    pinMode(PIN_PGD, OUTPUT);
    uint16_t frame = (data & 0x3FFF) << 1;
    for (int i = 0; i < 16; i++) {
        sendBit((frame >> i) & 0x01);
    }
    delayMicroseconds(3);
}

uint16_t readData14() {
    pinMode(PIN_PGD, INPUT);
    uint16_t raw = 0;
    for (int i = 0; i < 16; i++) {
        if (readBit()) raw |= (1 << i);
    }
    delayMicroseconds(3);
    return (raw >> 1) & 0x3FFF;
}

void enterProgMode() {
    pinMode(PIN_PGC, OUTPUT);
    pinMode(PIN_PGD, OUTPUT);
    digitalWrite(PIN_PGC, LOW);
    digitalWrite(PIN_PGD, LOW);
}

void exitProgMode() {
    pinMode(PIN_PGD, INPUT);
    pinMode(PIN_PGC, INPUT);
    current_pc = -1;
}

void seekAddress(uint16_t target) {
    if (target >= 0x8000) {
        if (current_pc < 0x8000 || current_pc > (int32_t)target) {
            sendCmd(CMD_LOAD_CONFIG);
            sendData14(0x3FFF);
            current_pc = 0x8000;
        }
        while (current_pc < (int32_t)target) {
            sendCmd(CMD_INC_ADDRESS);
            current_pc++;
        }
    } else {
        if (current_pc < 0 || current_pc > (int32_t)target || current_pc >= 0x8000) {
            sendCmd(CMD_RESET_ADDRESS);
            current_pc = 0;
        }
        while (current_pc < (int32_t)target) {
            sendCmd(CMD_INC_ADDRESS);
            current_pc++;
        }
    }
}

uint16_t readDeviceID() {
    enterProgMode();
    sendCmd(CMD_LOAD_CONFIG);
    sendData14(0x0000);
    for (int i = 0; i < 6; i++) {
        sendCmd(CMD_INC_ADDRESS);
    }
    sendCmd(CMD_READ_DATA_PROG);
    uint16_t id = readData14();
    current_pc = 0x8006;
    return id;
}

void bulkErase() {
    enterProgMode();
    sendCmd(CMD_LOAD_CONFIG);
    sendData14(0x3FFF);
    sendCmd(CMD_BULK_ERASE_PROG);
    delay(15);
    sendCmd(CMD_BULK_ERASE_DATA);
    delay(15);
    current_pc = -1;
}

void setup() {
    Serial.begin(115200);
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
    else if (line.startsWith("WPROG:")) {
        int p1 = line.indexOf(':');
        int p2 = line.indexOf(':', p1 + 1);
        uint16_t addr = strtol(line.substring(p1 + 1, p2).c_str(), NULL, 16);
        uint16_t val  = strtol(line.substring(p2 + 1).c_str(), NULL, 16);

        enterProgMode();
        seekAddress(addr);
        sendCmd(CMD_LOAD_DATA_PROG);
        sendData14(val);
        sendCmd(CMD_BEGIN_INT_TIMED_PRG);
        delay(4); // T_PINT: typisch 2.5ms
        sendCmd(CMD_INC_ADDRESS);
        current_pc++;

        Serial.println("OK");
    }
    else if (line == "DONE") {
        exitProgMode();
        Serial.println("OK DONE");
    }
}
