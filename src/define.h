#ifndef DEFINE_H
#define DEFINE_H

// USART1 for GSM module (PA9=TX, PA10=RX)
#define GSM_SERIAL Serial1
#define OLED_ADDR 0x3C
#define MAX30102_ADDR 0x57
#define DHTTYPE DHT22
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define STATUS_LED_Pin PC13
#define RELAY_FENCE_Pin PA5
#define RELAY_LAMP_Pin PA6
#define RELAY_FAN_Pin PA7
#define TDS_SENSOR_Pin PB0
#define TH_SENSOR_Pin PB1
#define GSM_RX_Pin PA9
#define GSM_TX_Pin PA10
#define MCU_SCL_Pin PB6
#define MCU_SDA_Pin PB7




// Bit manipulation macros
#define BITREAD(value, bit) ((value >> bit) & 0x01) // Macro to read a specific bit from a value
#define BITSET(value, bit) (value |= (1 << bit)) // Macro to set a specific bit in a value
#define BITCLEAR(value, bit) (value &= ~(1 << bit)) // Macro to clear a specific bit in a value
#define BITTOGGLE(value, bit) (value ^= (1 << bit)) // Macro to toggle


//Enum definitions
enum STATUS{
    NONE,
    SEND_STATUS,
    SEND_CONF,
    UPDT_CONF,
    READ_SMS
};

#endif // DEFINE_H