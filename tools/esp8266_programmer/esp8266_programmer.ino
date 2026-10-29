/*
 * ESP8266 ICSP Programmer für Microchip PIC16LF1938 (IKEA Bekant)
 * Unterstützt sowohl LVP als auch High-Voltage Entry (9V an MCLR).
 *
 * Verkabelung für 9V-Batterie-Trick:
 * 1. ESP8266 D5 NICHT mit Pad 1 verbinden! (D5 bleibt frei).
 * 2. 9V-Batterie MINUS (-) fest mit ESP8266 GND / Pad 3 verbinden (Gemeinsame Masse).
 * 3. 9V-Batterie PLUS (+) (über ca. 1k-10k Widerstand) an Pad 1 (MCLR) halten.
 *
 * 4. ESP8266 3V3 an Pad 2 (VDD)
 * 5. ESP8266 GND an Pad 3 (VSS)
 * 6. ESP8266 D6 (GPIO 12) an Pad 4 (PGD)
 * 7. ESP8266 D7 (GPIO 13) an Pad 5 (PGC)
 */
#include <Arduino.h>

#define PIN_MCLR  14  // Pin D5 (bleibt bei 9V-Trick unverbunden)
#define PIN_PGD   12  // Pin D6
#define PIN_PGC   13  // Pin D7

// ICSP Kommandos (6-Bit, LSB first) laut DS41397
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
    // 1 Start-Bit (0) + 14 Daten-Bits (LSB first) + 1 Stop-Bit (0) = 16 Takte
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
    // PGC und PGD initial auf LOW
    pinMode(PIN_PGC, OUTPUT);
    pinMode(PIN_PGD, OUTPUT);
    digitalWrite(PIN_PGC, LOW);
    digitalWrite(PIN_PGD, LOW);
    delay(5);
}

void exitProgMode() {
    pinMode(PIN_PGD, INPUT);
    pinMode(PIN_PGC, INPUT);
}

uint16_t readDeviceID() {
    sendCmd(CMD_LOAD_CONFIG);
    sendData14(0x0000); // Setzt PC auf 0x8000
    for (int i = 0; i < 6; i++) {
        sendCmd(CMD_INC_ADDRESS); // Vorrücken auf 0x8006 (Device ID Register)
    }
    sendCmd(CMD_READ_DATA_PROG);
    return readData14();
}

void bulkErase() {
    sendCmd(CMD_LOAD_CONFIG);
    sendData14(0x3FFF);
    sendCmd(CMD_BULK_ERASE_PROG);
    delay(15); // T_ERAB = max 10 ms
    sendCmd(CMD_BULK_ERASE_DATA);
    delay(15);
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
        enterProgMode();
        uint16_t id = readDeviceID();
        exitProgMode();
        Serial.printf("OK ID:0x%04X\n", id);
    }
    else if (line == "ERASE") {
        enterProgMode();
        bulkErase();
        exitProgMode();
        Serial.println("OK ERASED");
    }
    else if (line.startsWith("WPROG:")) {
        int p1 = line.indexOf(':');
        int p2 = line.indexOf(':', p1 + 1);
        uint16_t addr = strtol(line.substring(p1 + 1, p2).c_str(), NULL, 16);
        uint16_t val  = strtol(line.substring(p2 + 1).c_str(), NULL, 16);

        enterProgMode();
        if (addr >= 0x8000) {
            sendCmd(CMD_LOAD_CONFIG);
            sendData14(0x3FFF);
            for (uint16_t a = 0x8000; a < addr; a++) sendCmd(CMD_INC_ADDRESS);
        } else {
            sendCmd(CMD_RESET_ADDRESS);
            for (uint16_t a = 0; a < addr; a++) sendCmd(CMD_INC_ADDRESS);
        }
        sendCmd(CMD_LOAD_DATA_PROG);
        sendData14(val);
        sendCmd(CMD_BEGIN_INT_TIMED_PRG);
        delay(6);
        exitProgMode();
        Serial.println("OK");
    }
}
