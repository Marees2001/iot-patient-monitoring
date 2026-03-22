
#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <stdint.h>
#include <stdio.h>
#include <Arduino.h>
#include "define.h"
#include <Wire.h>
#include "MAX30100_PulseOximeter.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "DHT.h"

extern char gsm_rx_buffer[200]; // Buffer for receiving data from GSM module
extern uint8_t receive_bytes; // Number of bytes received from GSM module

/*Structure for controller status*/
struct CONTROLLER_STATUS {
    uint8_t init_gsm; // GSM initialization status by bitread operations: 0: GSM module initialized,
    //1: SIM card ready, 2: UART configured, 3: RSSI received,4: set SMS mode, 5: set SMS recipient
    float hrt_value; // TDS sensor value
    int spo_value; // TDS sensor value
    float temp_value; // Temperature sensor value
    float hum_value; // Humidity sensor value
    uint8_t rssi; // GSM signal strength (0-31)
    uint8_t sim_status; // SIM card status (0: not inserted, 1: ready)
    uint8_t alm_sta; // Alarm status (0 or 1)
};

/*Structure for controller settings*/
struct CONTROLLER_SETTINGS {
    uint8_t mx_temp; // Max threshold for temperature
    uint8_t mn_temp; // Min threshold for temperature
    uint8_t mx_hum; // Max threshold for humidity
    uint8_t mn_hum; // Min threshold for humidity
    uint8_t mx_hrt; // Max threshold for TDS; 
    //range 0-1000 ppm, where 0-300 ppm is considered good, 300-600 ppm is fair, and above 600 ppm is poor water quality
    uint8_t mn_hrt; // Min threshold for TDS
    uint8_t mx_spo; // Max threshold for TDS; 
    //range 0-1000 ppm, where 0-300 ppm is considered good, 300-600 ppm is fair, and above 600 ppm is poor water quality
    uint8_t mn_spo; // Min threshold for TDS
};

/*Structure for controller operation*/
struct CONTROLLER_OPERATION {
    uint8_t lamp_ctrl; // Lamp control (0 or 1)
    uint8_t fan_ctrl; // Fan control (0 or 1)
    uint8_t fence_ctrl; // Fence control (0 or 1) 
    uint8_t alarm_ctrl; // Alarm control (0 or 1)
    uint8_t valve_ctrl; // Valve control (0 or 1)
    uint8_t gsm_call_ctrl; // GSM call control (0 or 1)
    uint8_t gsm_sms_ctrl; // GSM SMS control (0 or 1)
};

struct IOT_PATIENT_MONITORING {
    struct CONTROLLER_STATUS sta; // Current status of the controller
    struct CONTROLLER_SETTINGS conf; // Current settings of the controller
    struct CONTROLLER_OPERATION opt; // Current operation of the controller
}ipm;

void init_controler(){
    // Initialize LED pin
    pinMode(STATUS_LED_Pin, OUTPUT);
    digitalWrite(STATUS_LED_Pin, LOW); // LED off (inverted logic on Blue Pill)

    pinMode(TH_SENSOR_Pin,INPUT_PULLUP);

    pinMode(MCU_SDA_Pin,INPUT_PULLUP);
    pinMode(MCU_SCL_Pin,INPUT_PULLUP);
    
}

// GSM function for send data via UART
void gsm_send_data(const char* data){
    Serial.print(">> Sending: ");
    Serial.println(data);
    GSM_SERIAL.print(data);
    GSM_SERIAL.print("\r\n");
}

// GSM function for receive data via UART
void gsm_receive_data(){
    // Re-arm DMA reception for the next payload
    memset(gsm_rx_buffer, 0, sizeof(gsm_rx_buffer));
    // Check for GSM responses
    if(GSM_SERIAL.available()) {
        int size = GSM_SERIAL.readBytes(gsm_rx_buffer, sizeof(gsm_rx_buffer) - 1); // Read GSM response into buffer
        if (size >= sizeof(gsm_rx_buffer) - 1) {
            memset(gsm_rx_buffer, 0, sizeof(gsm_rx_buffer)); // Clear buffer if overflow
            Serial.println("GSM receive buffer overflow, clearing buffer");
        }
         Serial.printf("GSM received: %s\n\r", gsm_rx_buffer); // Print the received GSM response for debu
    }

    // Process the received data as needed
    if(!BITREAD(ipm.sta.init_gsm, 0) && strstr(gsm_rx_buffer, "OK")) {
        BITSET(ipm.sta.init_gsm, 0); // Basic command to check communication
    } else if(!BITREAD(ipm.sta.init_gsm, 1) && strstr(gsm_rx_buffer, "+CSMINS:")) {
        sscanf(gsm_rx_buffer, "+CSMINS: %*d,%d", &ipm.sta.sim_status);
        BITSET(ipm.sta.init_gsm, 1); // Mark SIM card status as received
    } else if(!BITREAD(ipm.sta.init_gsm, 3) && strstr(gsm_rx_buffer, "+CSQ:")) {
        sscanf(gsm_rx_buffer, "+CSQ: %d,%*d", &ipm.sta.rssi);
        BITSET(ipm.sta.init_gsm, 3); // Mark RSSI as received
    } else if(!BITREAD(ipm.sta.init_gsm, 4) && strstr(gsm_rx_buffer,"OK")) {
        BITSET(ipm.sta.init_gsm, 4); // Mark SMS mode set as done
    } else if(!BITREAD(ipm.sta.init_gsm, 5) && strstr(gsm_rx_buffer,">")) {
        BITSET(ipm.sta.init_gsm, 5); // Mark SMS recipient set as done
    }else if(!BITREAD(ipm.sta.init_gsm, 6) && strstr(gsm_rx_buffer, ">")) {
        BITSET(ipm.sta.init_gsm, 6); // Mark test SMS sent as done
    } else if(!BITREAD(ipm.sta.init_gsm, 7) && strstr(gsm_rx_buffer, "OK")) {
        BITSET(ipm.sta.init_gsm, 7); // Mark end of SMS message set as done
    }
}

// Send initialization commands to GSM module in sequence and update status bits accordingly
void sync_gsm() {
    BITSET(ipm.sta.init_gsm, 2); // Mark UART configured as done
	Serial.printf("GSM Send cmd -%x\n\r",ipm.sta.init_gsm);
    if(!BITREAD(ipm.sta.init_gsm, 0)) gsm_send_data("ATE0"); // Basic command to check communication
    else if(!BITREAD(ipm.sta.init_gsm, 1)) gsm_send_data("AT+CSMINS?"); // Check SIM card status
    else if(!BITREAD(ipm.sta.init_gsm, 3)) gsm_send_data("AT+CSQ"); // Check signal quality
    else if(!BITREAD(ipm.sta.init_gsm, 4)) gsm_send_data("AT+CMGF=1"); // Set SMS mode
    else if(!BITREAD(ipm.sta.init_gsm, 5)) gsm_send_data("AT+CMGS=\"+919444861686\""); // Set SMS recipient
    else if(!BITREAD(ipm.sta.init_gsm, 6)) gsm_send_data("Hi Boss I am alive!"); // Sent test SMS
    else if(!BITREAD(ipm.sta.init_gsm, 7)) GSM_SERIAL.write(26); // set end of SMS message (Ctrl+Z)
}



#endif // CONTROLLER_H
