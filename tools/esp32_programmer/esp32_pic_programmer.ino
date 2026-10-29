/*
 * Low-Voltage ICSP Programmer für Microchip PIC16LF1938 (IKEA Bekant)
 * Unterstützt automatisch sowohl ESP32 als auch ESP8266 (NodeMCU / D1 Mini)
 * Protokoll nach Microchip DS41397 (14-Bit Enhanced Midrange, LVP Key: 0x4D434850 "MCHP")
 * Logikpegel: 3,3 V
 *
 * Verkabelung ESP32:
 * ESP32 GPIO 18 <-> ICSP Pad 1 (MCLR / VPP)
 * ESP32 3V3     <-> ICSP Pad 2 (VDD - ACHTUNG: NIEMALS an 5V / VIN!)
 * ESP32 GND     <-> ICSP Pad 3 (VSS)
 * ESP32 GPIO 19 <-> ICSP Pad 4 (PGD / ICSPDAT)
 * ESP32 GPIO 21 <-> ICSP Pad 5 (PGC / ICSPCLK)
 *
 * Verkabelung ESP8266 (NodeMCU / Wemos D1 Mini):
 * ESP8266 D5 (GPIO 14) <-> ICSP Pad 1 (MCLR / VPP)
 * ESP8266 3V3 (3.3V)   <-> ICSP Pad 2 (VDD - ACHTUNG: NIEMALS an 5V / VIN!)
 * ESP8266 G   (GND)    <-> ICSP Pad 3 (VSS)
 * ESP8266 D6 (GPIO 12) <-> ICSP Pad 4 (PGD / ICSPDAT)
 * ESP8266 D7 (GPIO 13) <-> ICSP Pad 5 (PGC / ICSPCLK)
 */
#include <Arduino.h>

#if defined(ESP8266)
  // Pinbelegung für ESP8266
  #define PIN_MCLR  14  // Pin D5
  #define PIN_PGD   12  // Pin D6
  #define PIN_PGC   13  // Pin D7
#else
  // Pinbelegung für ESP32
  #define PIN_MCLR  18
  #define PIN_PGD   19
  #define PIN_PGC   21
#endif

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

void enterLVP() {
    pinMode(PIN_MCLR, OUTPUT);
    pinMode(PIN_PGC, OUTPUT);
    pinMode(PIN_PGD, OUTPUT);

    digitalWrite(PIN_PGC, LOW);
    digitalWrite(PIN_PGD, LOW);
    digitalWrite(PIN_MCLR, HIGH);
    delay(10);
    digitalWrite(PIN_MCLR, LOW);
    delayMicroseconds(500);

    // 32-Bit LVP Key 0x4D434850 ("MCHP") LSB first einschieben + 1 Extratakt
    uint32_t key = 0x4D434850;
    for (int i = 0; i < 32; i++) {
        sendBit((key >> i) & 0x01);
    }
    sendBit(0);
    delay(5);
}

void exitLVP() {
    pinMode(PIN_PGD, INPUT);
    pinMode(PIN_PGC, INPUT);
    digitalWrite(PIN_MCLR, HIGH);
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
    delay(10); // T_ERAB = max 10 ms
    sendCmd(CMD_BULK_ERASE_DATA);
    delay(10);
}

void setup() {
    Serial.begin(115200);
    pinMode(PIN_MCLR, OUTPUT);
    digitalWrite(PIN_MCLR, HIGH);
}

void loop() {
#if defined(ESP8266)
    yield();
#endif
    if (!Serial.available()) return;
    String line = Serial.readStringUntil('\n');
    line.trim();

    if (line == "ID") {
        enterLVP();
        uint16_t id = readDeviceID();
        exitLVP();
        Serial.printf("OK ID:0x%04X\n", id);
    }
    else if (line == "ERASE") {
        enterLVP();
        bulkErase();
        exitLVP();
        Serial.println("OK ERASED");
    }
    else if (line.startsWith("WPROG:")) {
        int p1 = line.indexOf(':');
        int p2 = line.indexOf(':', p1 + 1);
        uint16_t addr = strtol(line.substring(p1 + 1, p2).c_str(), NULL, 16);
        uint16_t val  = strtol(line.substring(p2 + 1).c_str(), NULL, 16);

        enterLVP();
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
        exitLVP();
        Serial.println("OK");
    }
}
