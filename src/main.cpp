#include <Arduino.h>
#include "controller.h"

PulseOximeter hbt;
DHT dht(TH_SENSOR_Pin, DHTTYPE);
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS); // I2C LCD display at address 0x27, 16 columns, 4 rows

// I2C address (try 0x3C if not worki
TwoWire i2c(MCU_SDA_Pin,MCU_SCL_Pin);

char gsm_rx_buffer[200]; // Buffer for receiving data from GSM module
uint8_t receive_bytes = 0;
uint8_t pulse_sensor_status = 0; // 0: not initialized, 1: initialized successfully, 2: initialization failed

void read_th_sensor();
void read_hrt_pulse();

void setup() {
  // Initialize USB Serial (CDC) for USB debugging
  Serial.begin(9600);
  delay(10000); // Wait for USB enumeration
  
  // Initialize USART1 for GSM module
  GSM_SERIAL.begin(9600);
  delay(500);

  init_controler();

  // put your setup code here, to run once:
  Serial.println("\n\n=== IoT Patient Monitoring System ===");
  Serial.println("System initialized successfully");
  // delay(5000);
  i2c.begin();  // SDA, SCL (VERY IMPORTANT)
  i2c.setClock(50000);  // 100kHz


  // Serial.println("I2C Scanner");
  // byte error, address;
  // for(address = 1; address < 127; address++) {
  //   i2c.beginTransmission(address);
  //   error = i2c.endTransmission();
  //   Serial.printf("I2C scane aadr-%x\n\r",address);
  //   if (error == 0) {
  //     Serial.print("Found at 0x");
  //     Serial.println(address, HEX);
  //   }
  //   delay(500);
  // }
  // Serial.printf("Scan completed...\n\r");

  init_display();

  dht.begin();
  hbt.setOnBeatDetectedCallback(onBeatDetected);
}

void loop() {
 
  static unsigned long gsm_sync_timeout = 0; // Variable to track GSM synchronization timing
  static unsigned long sensor_read_timeout = 0; // Variable to track sensor reading timing
  static unsigned long display_update_timeout = 0; // Variable to track display update timing
  static unsigned long init_pulse_sensor_timeout = 0; // Variable to track pulse sensor initialization timing
  static unsigned long display_timout = 0; // Variable to track display update timing
  static unsigned long gsm_call_timeout = 0; 
  static unsigned long gsm_sms_timeout = 0;
  /*Function for receiving data from GSM module*/
  gsm_receive_data();

  hbt.update(); // Update pulse oximeter readings

  if(millis() - gsm_sync_timeout > 2000) { // Check if 0.5 seconds have passed since the last GSM synchronization
    gsm_sync_timeout = millis(); // Update the last synchronization time
    // Toggle LED state using STM32 HAL
    digitalToggle(STATUS_LED_Pin);      // Push to OLED
    if(ipm.sta.init_gsm != 0xFF) { // Sync GSM every 0.1 seconds
      sync_gsm();
    }else {

      if(ipm.opt.gsm_sms_ctrl && (millis() - gsm_sms_timeout > 5000)) { // If GSM SMS control is active and 60 seconds have passed since the last SMS attempt
        gsm_sms_timeout = millis(); // Update the last SMS attempt time
        // Implement GSM SMS control logic here (e.g., send an SMS if not already sent)
        if(ipm.opt.gsm_sms_ctrl==1) {
          char buff[30];
          memset(buff,0,sizeof(buff));
          sprintf(buff,"AT+CMGS=\"%s\"",ipm.conf.home_mob);
          gsm_send_data(buff);
          delay(500);
          gsm_send_dashboard();
          delay(500);
          GSM_SERIAL.write(26); // Send Ctrl+Z to indicate end of SMS message
          ipm.opt.gsm_sms_ctrl = 2; // Mark SMS as sent
        } else if(ipm.opt.gsm_sms_ctrl==2) {
          ipm.opt.gsm_sms_ctrl = 0; // Reset SMS control to allow future SMS messages
          char buff[30];
          memset(buff,0,sizeof(buff));
          sprintf(buff,"AT+CMGS=\"%s\"",ipm.conf.emg_mob);
          gsm_send_data(buff);
          delay(500);
          gsm_send_dashboard();
          delay(500);
          GSM_SERIAL.write(26); // Send Ctrl+Z to indicate end of SMS message
        }
      }
      
      // Serial.println("GSM synchronization completed");
      if(ipm.opt.gsm_call_ctrl && (millis() - gsm_call_timeout > 30000)
      && !ipm.opt.gsm_sms_ctrl) { // If GSM call control is active and 30 seconds have passed since the last call attempt
        gsm_call_timeout = millis(); // Update the last call attempt time
        // Implement GSM call control logic here (e.g., make a call if not already in progress)
        if(ipm.opt.gsm_call_ctrl==1) {
          char buff[20];
          memset(buff,0,sizeof(buff));
          sprintf(buff,"ATD%s",ipm.conf.emg_mob);
          gsm_send_data(buff);
          ipm.opt.gsm_call_ctrl = 2; // Mark call as in progress
        } else if(ipm.opt.gsm_call_ctrl==2) {
          char buff[20];
          memset(buff,0,sizeof(buff));
          sprintf(buff,"ATD%s",ipm.conf.home_mob);
          gsm_send_data(buff);
          ipm.opt.gsm_call_ctrl = 0; // Reset call control to allow future calls
        }
      }
    }
    // Reading temperature or humidity takes about 250 milliseconds!
  }

  ipm.opt.alarm_ctrl? digitalWrite(ALARM_BUZZER_Pin, HIGH) 
  : digitalWrite(ALARM_BUZZER_Pin, LOW); // Set LED state based on alarm control

  
  if(millis()-init_pulse_sensor_timeout > 5000 && pulse_sensor_status == 0) { // Try to initialize pulse sensor every 10 seconds if not initialized
    init_pulse_sensor_timeout = millis(); // Update the last initialization attempt time
    pulse_sensor_status = hbt.begin();
    pulse_sensor_status? Serial.println("Pulse oximeter initialized successfully") 
    : Serial.println("ERROR: Failed to initialize pulse oximeter");
  }
  

  // put your main code here, to run repeatedly:
  if (Serial.available()) {
    char usbChar[200];
    Serial.readBytes(usbChar, sizeof(usbChar));
    Serial.print("USB received: ");
    Serial.println(usbChar);
    // Forward USB data to GSM
    GSM_SERIAL.print(usbChar);
    GSM_SERIAL.print("\r\n");
    Serial.println("Sending AT command to GSM...");
  }

  if(millis() - sensor_read_timeout > 1000){
    sensor_read_timeout = millis();
    read_th_sensor();
    read_hrt_pulse();
  }

  if(millis() - display_timout > 2000){
    //update_display();
    display_timout = millis();
    update_display();
  }
}


