
#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <stdint.h>
#include <stdio.h>
#include <Arduino.h>
#include "define.h"
#include <Wire.h>
#include "MAX30100_PulseOximeter.h"
#include <LiquidCrystal_I2C.h>
#include "DHT.h"
#include "flash.h"

extern char gsm_rx_buffer[200]; // Buffer for receiving data from GSM module
extern uint8_t receive_bytes; // Number of bytes received from GSM module
extern LiquidCrystal_I2C lcd; // I2C LCD display object
extern PulseOximeter hbt;
extern DHT dht; // DHT sensor object
extern uint8_t pulse_sensor_status;
extern uint8_t hbt_intrupt_status;

/*Structure for controller status*/
struct CONTROLLER_STATUS {
    uint8_t init_gsm; // GSM initialization status by bitread operations: 0: GSM module initialized,
    //1: SIM card ready, 2: UART configured, 3: RSSI received,4: set SMS mode, 5: set SMS recipient
    float hrt_value; // TDS sensor value
    int spo_value; // TDS sensor value
    float temp_value; // Temperature sensor value
    uint8_t hum_value; // Humidity sensor value
    uint8_t rssi; // GSM signal strength (0-31)
    uint8_t sim_status; // SIM card status (0: not inserted, 1: ready)
    uint8_t alm_sta; // Alarm status (0 or 1)
    uint8_t lst_msg_idx;
};

/*Structure for controller settings*/
struct CONTROLLER_SETTINGS {
    uint32_t magic;
    char home_mob[15]; // Home mobile number for GSM communication
    char emg_mob[15]; // Emergency mobile number for GSM communication
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
    uint8_t alarm_ctrl; // Alarm control (0 or 1)
    uint8_t gsm_call_ctrl; // GSM call control (0 or 1)
    uint8_t gsm_sms_ctrl; // GSM SMS control (0 or 1)
    char lst_mob_req[15];
};

struct IOT_PATIENT_MONITORING {
    struct CONTROLLER_STATUS sta; // Current status of the controller
    struct CONTROLLER_SETTINGS conf; // Current settings of the controller
    struct CONTROLLER_OPERATION opt; // Current operation of the controller
}ipm;


void init_display(){
    lcd.init();
    lcd.backlight();
    lcd.blink_off();
    lcd.setCursor(2, 0);
    lcd.print("IOT Patient");
    lcd.setCursor(3, 1);
    lcd.print("Monitoring");
    delay(2000);
}

void TH_detect() {
    // This function can be used to handle any specific logic when the temperature/humidity sensor is triggered
    // For example, you could read the sensor values and update the display or send an alert
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    ipm.sta.temp_value = dht.readTemperature();
    // Read temperature as Celsius (the default)
    ipm.sta.hum_value = dht.readHumidity();
    Serial.println("Temperature/Humidity sensor triggered");
}


void onBeatDetected() {
    Serial.println("Beat detected!");
    hbt_intrupt_status = 1; // Set the heartbeat interrupt status to indicate a beat was detected
}

void update_display() {
   lcd.clear();

    // Row 1
    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(ipm.sta.temp_value, 1); // 1 decimal
    lcd.print("C ");

    lcd.print("H:");
    lcd.print(ipm.sta.hum_value, 0);
    lcd.print("%");

    // Row 2
    lcd.setCursor(0, 1);
    lcd.print("HR:");
    lcd.print(ipm.sta.hrt_value);
    lcd.print(" ");

    lcd.print("S:");
    lcd.print(ipm.sta.spo_value);
    lcd.print("%");
}

void read_th_sensor(){
  Serial.println("Tempratur : " + String(ipm.sta.temp_value) + "C");
  Serial.println("Humitidy : " + String(ipm.sta.hum_value) + "%");

  if((ipm.sta.temp_value > ipm.conf.mx_temp || ipm.sta.temp_value < ipm.conf.mn_temp)
  && ipm.sta.temp_value != 0) {
    ipm.opt.alarm_ctrl = 1; // Activate alarm control
    ipm.opt.gsm_call_ctrl = 1; // Activate GSM call control
    ipm.opt.gsm_sms_ctrl = 1; // Activate GSM SMS control
  } else if((ipm.sta.hum_value > ipm.conf.mx_hum || ipm.sta.hum_value < ipm.conf.mn_hum)
  && ipm.sta.hum_value != 0) {
    ipm.opt.alarm_ctrl = 1; // Activate alarm control if humidity is out of range
    ipm.opt.gsm_call_ctrl = 1; // Activate GSM call control
    ipm.opt.gsm_sms_ctrl = 1; // Activate GSM SMS control 
  } else {
    ipm.opt.alarm_ctrl = 0; // Clear alarm control if all readings are normal
    ipm.opt.gsm_call_ctrl = 0; // Clear GSM call control
    ipm.opt.gsm_sms_ctrl = 0; // Clear GSM SMS control
  }

}

void read_hrt_pulse(){
    ipm.sta.hrt_value = hbt.getHeartRate();
    ipm.sta.spo_value = hbt.getSpO2();
    Serial.println("Heart rate : " + String(ipm.sta.hrt_value)+" bpm");
    Serial.println("SPO2  rate : " + String(ipm.sta.spo_value) + " %");
    if((ipm.sta.hrt_value > ipm.conf.mx_hrt || ipm.sta.hrt_value < ipm.conf.mn_hrt) 
    && ipm.sta.hrt_value != 0) {
        ipm.opt.alarm_ctrl = 1; // Activate alarm control
        ipm.opt.gsm_call_ctrl = 1; // Activate GSM call control
        ipm.opt.gsm_sms_ctrl = 1; // Activate GSM SMS control   
    } else if((ipm.sta.spo_value > ipm.conf.mx_spo || ipm.sta.spo_value < ipm.conf.mn_spo) 
    && ipm.sta.spo_value != 0) {
        ipm.opt.alarm_ctrl = 1; // Activate alarm control if SpO2 is out of range
        ipm.opt.gsm_call_ctrl = 1; // Activate GSM call control
        ipm.opt.gsm_sms_ctrl = 1; // Activate GSM SMS control 
    } else {
        ipm.opt.alarm_ctrl = 0; // Clear alarm control if all readings are normal
        ipm.opt.gsm_call_ctrl = 0; // Clear GSM call control
        ipm.opt.gsm_sms_ctrl = 0; // Clear GSM SMS control
    }
}

// GSM function for send data via UART
void gsm_send_data(const char* data){
    Serial.print(">> Sending: ");
    Serial.println(data);
    GSM_SERIAL.print(data);
    GSM_SERIAL.print("\r\n");
}

void gsm_send_dashboard(){
    char buff[200];
    memset(buff,0,sizeof(buff));
    sprintf(buff,"1.Temprature:%.2f C  2.Humidity:%00d%% 3.Heartbeat: %d bpm 4.SPO2: %d%%",
        ipm.sta.temp_value,ipm.sta.hum_value,ipm.sta.hrt_value,ipm.sta.spo_value);
    gsm_send_data(buff);
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
    }else if(strstr(gsm_rx_buffer, "+CMTI:")) {
        int lst_sms_idx = 0;
         char *cmti_ptr = strstr(gsm_rx_buffer, "+CMTI:");
        if(cmti_ptr) {
            sscanf(cmti_ptr, "+CMTI: %*[^,],%d", &lst_sms_idx);
        }
        char buff[20];
        memset(buff,0,sizeof(buff));
        sprintf(buff,"AT+CMGR=%d",lst_sms_idx);
        gsm_send_data(buff);
        Serial.printf("lst_sms_idx->%d\n\r",lst_sms_idx);
    }else if(strstr(gsm_rx_buffer, "+CMGR:")) {
        char *cmgr_ptr = strstr(gsm_rx_buffer, "+CMGR:");
        Serial.printf("RAW: [%s]\n\r", cmgr_ptr);
        if(cmgr_ptr) {
            sscanf(cmgr_ptr, "+CMGR: %*[^,],\"%[^\"]\"", ipm.opt.lst_mob_req);
            Serial.printf("Phone parsed: [%s]\n\r", ipm.opt.lst_mob_req);
        }
        if(strstr(gsm_rx_buffer, "Dashboard")) {
            char buff[30];
            memset(buff,0,sizeof(buff));
            sprintf(buff,"AT+CMGS=\"%s\"",ipm.opt.lst_mob_req);
            gsm_send_data(buff);
            delay(500);
            gsm_send_dashboard();
            delay(500);
            GSM_SERIAL.write(26);
        }else if(strstr(gsm_rx_buffer, "Modify")){
            char buff[30];
            memset(buff,0,sizeof(buff));
            sprintf(buff,"AT+CMGS=\"%s\"",ipm.opt.lst_mob_req);
            gsm_send_data(buff);
            delay(500);
            gsm_send_data("What you want to modify? 1.Contact 2.Sensors");
            delay(500);
            GSM_SERIAL.write(26);
        }else if(strstr(gsm_rx_buffer, "1.Contact")) {
            char buff[30];
            memset(buff,0,sizeof(buff));
            sprintf(buff,"AT+CMGS=\"%s\"",ipm.opt.lst_mob_req);
            gsm_send_data(buff);
            delay(500);
            gsm_send_data("What you want to modify? 1.Owner: 2.Doctor:");
            delay(500);
            GSM_SERIAL.write(26);
        }else if(strstr(gsm_rx_buffer, "1.Owner:")) {
            char buff[100],num[15];
            memset(num,0,sizeof(num));
            char *owner_ptr = strstr(gsm_rx_buffer, "1.Owner:");
            if(owner_ptr) {
                sscanf(owner_ptr, "1.Owner: %s", num);  // num not &num
                // trim any \r\n at end
                num[strcspn(num, "\r\n")] = 0;
                Serial.printf("Owner num: [%s]\n\r", num);
                memccpy(ipm.conf.home_mob, num, '\0', sizeof(ipm.conf.home_mob)); // Store the new home mobile number in configuration
                save_to_flash(&ipm.conf,sizeof(ipm.conf));
            }

            // memset(buff, 0, sizeof(buff));
            // sprintf(buff, "AT+CPBW=1,\"%s\",145,\"Owner\"", num);
            // gsm_send_data(buff);
            // delay(500);
            memset(buff,0,sizeof(buff));
            sprintf(buff,"AT+CMGS=\"%s\"",ipm.opt.lst_mob_req);
            gsm_send_data(buff);
            delay(500);
            gsm_send_data("The modified successfully!!");
            delay(500);
            GSM_SERIAL.write(26);
        }else if(strstr(gsm_rx_buffer, "2.Doctor:")) {
            char buff[100],num[15];
            memset(num,0,sizeof(num));
            char *owner_ptr = strstr(gsm_rx_buffer, "1.Doctor:");
            if(owner_ptr) {
                sscanf(owner_ptr, "1.Doctor: %s", num);  // num not &num
                // trim any \r\n at end
                num[strcspn(num, "\r\n")] = 0;
                Serial.printf("Doctor num: [%s]\n\r", num);
                memccpy(ipm.conf.emg_mob, num, '\0', sizeof(ipm.conf.emg_mob)); // Store the new emergency mobile number in configuration
                save_to_flash(&ipm.conf,sizeof(ipm.conf)); // Save the updated configuration to flash memory
            }

            // memset(buff, 0, sizeof(buff));
            // sprintf(buff, "AT+CPBW=1,\"%s\",145,\"Doctor\"", num);
            // gsm_send_data(buff);
            // delay(500);
            memset(buff,0,sizeof(buff));
            sprintf(buff,"AT+CMGS=\"%s\"",ipm.opt.lst_mob_req);
            gsm_send_data(buff);
            delay(500);
            gsm_send_data("The modified successfully!!");
            delay(500);
            GSM_SERIAL.write(26);
        }else if(strstr(gsm_rx_buffer, "2.Sensors")) {
            char buff[30];
            memset(buff,0,sizeof(buff));
            sprintf(buff,"AT+CMGS=\"%s\"",ipm.opt.lst_mob_req);
            gsm_send_data(buff);
            delay(500);
            gsm_send_data("I can help you.What I do? 1.Set T/H 2.Set TDS");
            delay(500);
            GSM_SERIAL.write(26);
        }else if(strstr(gsm_rx_buffer, "1.Set T/H")){
            char buff[30];
            memset(buff,0,sizeof(buff));
            sprintf(buff,"AT+CMGS=\"%s\"",ipm.opt.lst_mob_req);
            gsm_send_data(buff);
            delay(500);
            gsm_send_data("Fill the value. 1.Tmax: 2.Tmin: 3.Hmax: 4.Hmin:");
            delay(500);
            GSM_SERIAL.write(26);
        }else if(strstr(gsm_rx_buffer, "2.Set SPO")){
            char buff[30];
            memset(buff,0,sizeof(buff));
            sprintf(buff,"AT+CMGS=\"%s\"",ipm.opt.lst_mob_req);
            gsm_send_data(buff);
            delay(500);
            gsm_send_data("Fill the value. 1.HRTmax: 2.HRTmin: 3.SPO2max: 4.SPO2min:");
            delay(500);
            GSM_SERIAL.write(26);
        }else if(strstr(gsm_rx_buffer, "Fill the value. 1.Tmax:")) {
            char buff[30];
            sscanf(gsm_rx_buffer, "Fill the value. 1.Tmax:%f 2.Tmin:%f 3.Hmax:%d 4.Hmin:%d",
                &ipm.conf.mx_temp,&ipm.conf.mn_temp,&ipm.conf.mx_hum,&ipm.conf.mn_hum);
            memset(buff,0,sizeof(buff));
            sprintf(buff,"AT+CMGS=\"%s\"",ipm.opt.lst_mob_req);
            gsm_send_data(buff);
            delay(500);
            gsm_send_data("The modified successfully!!");
            delay(500);
            GSM_SERIAL.write(26);
            Serial.printf("1.Tmax:%f 2.Tmin:%f 3.Hmax:%d 4.Hmin:%d\r\n",
            ipm.conf.mx_temp,ipm.conf.mn_temp,ipm.conf.mx_hum,ipm.conf.mn_hum);
            //Store the value
            save_to_flash(&ipm.conf,sizeof(ipm.conf));
        }else if(strstr(gsm_rx_buffer, "Fill the value. 1.HRTmax:")) {
            char buff[30];
            sscanf(gsm_rx_buffer, "Fill the value. 1.HRTmax:%d 2.HRTmin:%d 3.SPO2max:%d 4.SPO2min:%d",
                &ipm.conf.mx_hrt,&ipm.conf.mn_hrt,&ipm.conf.mx_spo,&ipm.conf.mn_spo);
            memset(buff,0,sizeof(buff));
            sprintf(buff,"AT+CMGS=\"%s\"",ipm.opt.lst_mob_req);
            gsm_send_data(buff);
            delay(500);
            gsm_send_data("The modified successfully!!");
            delay(500);
            GSM_SERIAL.write(26);
            Serial.printf("1.HRTmax:%d 2.HRTmin:%d 3.SPO2max:%d 4.SPO2min:%d\r\n",
            ipm.conf.mx_hrt,ipm.conf.mn_hrt,ipm.conf.mx_spo,ipm.conf.mn_spo);
            //Store the value
            save_to_flash(&ipm.conf,sizeof(ipm.conf));
        }else if(strstr(gsm_rx_buffer, "Location")) {
            char buff[100];
            memset(buff,0,sizeof(buff));
            String lat = "12.978208332240508";
            String lon = "79.96812928984802";

            sprintf(buff,"AT+CMGS=\"%s\"",ipm.opt.lst_mob_req);
            gsm_send_data(buff);
            delay(500);
            memset(buff,0,sizeof(buff));
            sprintf(buff, "https://www.google.com/maps?q=%s,%s", lat.c_str(), lon.c_str());
            gsm_send_data(buff);
            delay(500);
            GSM_SERIAL.write(26);
            Serial.printf("Sent location: %s\n\r", buff);
        }

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

void init_controler(){
    // Initialize LED pin
    pinMode(STATUS_LED_Pin, OUTPUT);
    digitalWrite(STATUS_LED_Pin, LOW); // LED off (inverted logic on Blue Pill)

    pinMode(TH_SENSOR_Pin,INPUT_PULLUP);
    pinMode(ALARM_BUZZER_Pin, OUTPUT);
    pinMode(USER_BUTTON_Pin, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(TH_SENSOR_Pin), TH_detect, FALLING); // Attach interrupt to user button for temperature/humidity detection
    
    read_from_flash(&ipm.conf,sizeof(ipm.conf)); // Load configuration from flash memory
    Serial.println("Controller initialized with following configuration:");
    Serial.printf("Home Mobile: %s\n\r", ipm.conf.home_mob);
    Serial.printf("Emergency Mobile: %s\n\r", ipm.conf.emg_mob);
    Serial.printf("Temperature Thresholds: Min=%d C, Max=%d C\n\r", ipm.conf.mn_temp, ipm.conf.mx_temp);
    Serial.printf("Humidity Thresholds: Min=%d %%, Max=%d %%\n\r", ipm.conf.mn_hum, ipm.conf.mx_hum);
    Serial.printf("Heart Rate Thresholds: Min=%d bpm, Max=%d bpm\n\r", ipm.conf.mn_hrt, ipm.conf.mx_hrt);
    Serial.printf("SpO2 Thresholds: Min=%d %%, Max=%d %%\n\r", ipm.conf.mn_spo, ipm.conf.mx_spo);
    
    ipm.conf.magic = FLASH_MAGIC;

}

#endif // CONTROLLER_H
