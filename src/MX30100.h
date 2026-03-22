
#define MAX30100_ADDRESS   0x57
#define REG_INTR_STATUS_1  0x00
#define REG_INTR_ENABLE_1  0x02
#define REG_FIFO_WR_PTR    0x04
#define REG_OVF_COUNTER    0x05
#define REG_FIFO_RD_PTR    0x06
#define REG_FIFO_DATA      0x07
#define REG_FIFO_CONFIG    0x08
#define REG_MODE_CONFIG    0x09
#define REG_SPO2_CONFIG    0x0A
#define REG_LED_CONFIG     0x0C
#define REG_PART_ID        0xFF

// ─────────────────────────────────────────
// STOP-THEN-START read (no repeated start)
// ─────────────────────────────────────────

void writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(MAX30100_ADDRESS);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission(true);
    delay(2);
}

uint8_t readRegister(uint8_t reg) {
    Wire.beginTransmission(MAX30100_ADDRESS);
    Wire.write(reg);
    Wire.endTransmission(true);   // full STOP
    delay(1);
    Wire.requestFrom((uint8_t)MAX30100_ADDRESS, (uint8_t)1);
    uint32_t t = millis() + 100;
    while (!Wire.available()) { if (millis() > t) return 0xFF; }
    return Wire.read();
}

void readMultipleBytes(uint8_t reg, uint8_t *buf, uint8_t len) {
    Wire.beginTransmission(MAX30100_ADDRESS);
    Wire.write(reg);
    Wire.endTransmission(true);   // full STOP
    delay(1);
    Wire.requestFrom((uint8_t)MAX30100_ADDRESS, (uint8_t)len);
    uint32_t t = millis() + 100;
    for (uint8_t i = 0; i < len; i++) {
        while (!Wire.available()) { if (millis() > t) { buf[i] = 0x00; break; } }
        buf[i] = Wire.available() ? Wire.read() : 0x00;
    }
}

// ─────────────────────────────────────────
// Init
// ─────────────────────────────────────────

void MAX30100_init() {
    writeRegister(REG_MODE_CONFIG, 0x40);  // reset
    delay(500);

    uint8_t mode = readRegister(REG_MODE_CONFIG);
    Serial.print("Mode after reset: 0x"); Serial.println(mode, HEX);
    // should be 0x00, not 0xFF

    writeRegister(REG_FIFO_WR_PTR, 0x00);
    writeRegister(REG_OVF_COUNTER, 0x00);
    writeRegister(REG_FIFO_RD_PTR, 0x00);

    writeRegister(REG_FIFO_CONFIG, 0x0F);
    writeRegister(REG_MODE_CONFIG, 0x03);  // SpO2 mode
    delay(100);
    writeRegister(REG_SPO2_CONFIG, 0x27);
    writeRegister(REG_LED_CONFIG,  0x66);  // 27.1mA each
    delay(100);

    Serial.print("Mode Config:  0x"); Serial.println(readRegister(REG_MODE_CONFIG), HEX);
    Serial.print("LED Config:   0x"); Serial.println(readRegister(REG_LED_CONFIG),  HEX);
    Serial.print("SpO2 Config:  0x"); Serial.println(readRegister(REG_SPO2_CONFIG), HEX);
}

// ─────────────────────────────────────────
// Read FIFO
// ─────────────────────────────────────────

bool MAX30100_readFIFO(uint16_t *ir, uint16_t *red) {
    uint8_t wr = readRegister(REG_FIFO_WR_PTR);
    uint8_t rd = readRegister(REG_FIFO_RD_PTR);

    if (wr == 0xFF || rd == 0xFF) return false;
    if (wr == rd) return false;  // no new samples

    uint8_t d[4];
    readMultipleBytes(REG_FIFO_DATA, d, 4);

    *red = ((uint16_t)d[0] << 8) | d[1];
    *ir  = ((uint16_t)d[2] << 8) | d[3];
    return true;
}


