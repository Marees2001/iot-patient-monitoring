#include <Arduino.h>
#include "controller.h"

PulseOximeter hbt;
MAX30100 hrm;
DHT dht(TH_SENSOR_Pin, DHTTYPE);
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS); // I2C LCD display at address 0x27, 16 columns, 4 rows

// I2C address (try 0x3C if not worki
TwoWire i2c(MCU_SDA_Pin,MCU_SCL_Pin);

char gsm_rx_buffer[200]; // Buffer for receiving data from GSM module
uint8_t receive_bytes = 0;
uint8_t pulse_sensor_status = 0; // 0: not initialized, 1: initialized successfully, 2: initialization failed
uint8_t hbt_intrupt_status = 0; // 0: no interrupt, 1: beat detected interrupt;

void onBeatDetected() {
  Serial.println("Beat detected!");
  hbt_intrupt_status = 1; // Set the heartbeat interrupt status to indicate a beat was detected
}

void read_th_sensor();
void read_hrt_pulse();


void setup() {
  // Initialize USB Serial (CDC) for USB debugging
  Serial.begin(115200);
  delay(7000); // Wait for USB enumeration
  
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

  if (!hbt.begin(PULSEOXIMETER_DEBUGGINGMODE_NONE)) {
    Serial.println("ERROR: Failed to initialize pulse oximeter");
    pulse_sensor_status = 0; // Mark as initialization faile
  }else {
    Serial.println("Pulse oximeter initialized successfully");
    pulse_sensor_status = 1; // Mark as initialization successful
    hbt.setOnBeatDetectedCallback(onBeatDetected);
  }

  dht.begin();
}

void loop() {
 
  static unsigned long gsm_sync_timeout = 0; // Variable to track GSM synchronization timing
  static unsigned long hbt_sensor_timeout = 0; // Variable to track sensor reading timing
  static unsigned long th_sensor_timeout = 0; // Variable to track sensor reading timing
  static unsigned long display_update_timeout = 0; // Variable to track display update timing
  static unsigned long init_pulse_sensor_timeout = 0; // Variable to track pulse sensor initialization timing
  static unsigned long display_timout = 0; // Variable to track display update timing
  static unsigned long gsm_call_timeout = 0; 
  static unsigned long gsm_sms_timeout = 0;
  static unsigned long gsm_clean_storage_timeout = 0;
  static unsigned long pulse_timout = 0; // Variable to track pulse sensor reading timing
  /*Function for receiving data from GSM module*/
  gsm_receive_data();
  
  if(pulse_sensor_status == 1) { // Read pulse oximeter data every 2 seconds
    hbt.update();
  }

  if(millis() > pulse_timout + 10000){
    pulse_timout = millis(); // Reset the pulse timeout to avoid resetting values immediately after a
    hbt.resume(); // Resume pulse oximeter to ensure it continues to read data
    hrm.resetFifo(); // Clear the FIFO buffer to avoid reading stale data
    Serial.println("Pulse oximeter data reset to ensure fresh readings");
  }


  if(millis() > gsm_sync_timeout + 2000) { // Check if 0.5 seconds have passed since the last GSM synchronization
    gsm_sync_timeout = millis(); // Update the last synchronization time
    // Toggle LED state using STM32 HAL
    digitalToggle(STATUS_LED_Pin);      // Push to OLED
    if(ipm.sta.init_gsm != 0xFF) { // Sync GSM every 0.1 seconds
      sync_gsm();
    }
    // Reading temperature or humidity takes about 250 milliseconds!
  }

  if(ipm.sta.init_gsm == 0xFF && !ipm.opt.gsm_sms_ctrl && !ipm.opt.gsm_call_ctrl
    && (millis() > gsm_clean_storage_timeout + 60000)) { // Clean GSM storage every 60 seconds if GSM is fully initialized
    gsm_clean_storage_timeout = millis(); // Update the last storage cleaning time
    clean_sim_storage();
    Serial.println("GSM storage cleaned");
  }

  if(ipm.opt.gsm_sms_ctrl && ipm.sta.init_gsm == 0xFF
  && (millis() > gsm_sms_timeout + 5000L)) { // If GSM SMS control is active and 60 seconds have passed since the last SMS attempt
    
    Serial.println("Attempting to send GSM SMS... Timout: " + String(gsm_sms_timeout) + " ms");
    Serial.println("GSM SMS control state: " + String(ipm.opt.gsm_sms_ctrl));
    gsm_sms_timeout = millis(); // Update the last SMS attempt timem 
    // Implement GSM SMS control logic here (e.g., send an SMS if not already sent)
    if(ipm.opt.gsm_sms_ctrl==1) {
      ipm.opt.gsm_sms_ctrl = 2; // Mark SMS as sent
      if(strlen(ipm.conf.home_mob) > 8) { // If home mobile number is configured, send SMS to it first
        char buff[30];
        memset(buff,0,sizeof(buff));
        sprintf(buff,"AT+CMGS=\"%s\"",ipm.conf.home_mob);
        gsm_send_data(buff);
        delay(500);
        gsm_send_dashboard();
        delay(500);
        GSM_SERIAL.write(26); // Send Ctrl+Z to indicate end of SMS message
      }
    } else if(ipm.opt.gsm_sms_ctrl == 2) {
      ipm.opt.gsm_sms_ctrl = 0; // Reset SMS control to allow future SMS messages
      gsm_sms_timeout = millis() + 120000L; // Update the last SMS attempt time to avoid sending multiple SMS messages in quick succession  
      if(strlen(ipm.conf.emg_mob) > 8) { // If emergency mobile number is configured, send SMS to it next
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
  }
  
  // Serial.println("GSM synchronization completed");
  if(ipm.opt.gsm_call_ctrl && !ipm.opt.gsm_sms_ctrl && ipm.sta.init_gsm == 0xFF
  && (millis() > gsm_call_timeout + 30000L)) { // If GSM call control is active and 30 seconds have passed since the last call attempt
    
    Serial.println("Attempting to make GSM call... Timout: " + String(gsm_call_timeout) + " ms");
    Serial.println("GSM call control state: " + String(ipm.opt.gsm_call_ctrl));
    gsm_call_timeout = millis(); // Update the last call attempt time
    // Implement GSM call control logic here (e.g., make a call if not already in progress)
    if(ipm.opt.gsm_call_ctrl==1) {
      ipm.opt.gsm_call_ctrl = 2; // Mark call as in progress
      if(strlen(ipm.conf.emg_mob) > 8) { // If emergency mobile number is configured, call it first
        char buff[20];
        memset(buff,0,sizeof(buff));
        sprintf(buff,"ATD%s;",ipm.conf.emg_mob);
        gsm_send_data(buff);
      }
      
    } else if(ipm.opt.gsm_call_ctrl==2) {
      ipm.opt.gsm_call_ctrl = 0; // Reset call control to allow future calls
      gsm_call_timeout = millis() + 120000L; // Update the last call attempt time to avoid making multiple calls in quick succession
      if(strlen(ipm.conf.home_mob) > 8) { // If home mobile number is configured, call it next
        char buff[20];
        memset(buff,0,sizeof(buff));
        sprintf(buff,"ATD%s;",ipm.conf.home_mob);
        gsm_send_data(buff);
      }
      
    }
    
  }
  
  if(digitalRead(USER_BUTTON_Pin) == LOW) { // Read digital inputs (e.g., user button)
    // Handle button press event
    //Serial.println("User button pressed");
    ipm.opt.alarm_ctrl = HIGH; // Toggle alarm control state on button press
    if(!ipm.opt.gsm_call_ctrl) ipm.opt.gsm_call_ctrl = 1; // Activate GSM call control
    if(!ipm.opt.gsm_sms_ctrl)  ipm.opt.gsm_sms_ctrl = 1; // Activate GSM SMS control 
  }else if(!ipm.sta.temp_alarm && !ipm.sta.hum_alarm && !ipm.sta.hrt_alarm && !ipm.sta.spo_alarm) {
    // Handle button release event
    // Serial.println("User button released");
    ipm.opt.alarm_ctrl = LOW; // Toggle alarm control state on button release
  }

  ipm.opt.alarm_ctrl? digitalWrite(ALARM_BUZZER_Pin, HIGH) 
  : digitalWrite(ALARM_BUZZER_Pin, LOW); // Set LED state based on alarm control

  
  if(millis() > init_pulse_sensor_timeout + 5000 && pulse_sensor_status == 0) { // Try to initialize pulse sensor every 10 seconds if not initialized
    init_pulse_sensor_timeout = millis(); // Update the last initialization attempt time
    pulse_sensor_status = hbt.begin(PULSEOXIMETER_DEBUGGINGMODE_NONE);
    if(pulse_sensor_status) {
      Serial.println("Pulse oximeter initialized successfully");
      hbt.setOnBeatDetectedCallback(onBeatDetected);
    } else {
      Serial.println("ERROR: Failed to initialize pulse oximeter");
    }
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

  if(millis() > th_sensor_timeout + 2000){
    th_sensor_timeout = millis();
    read_th_sensor();
  }

  if(hbt_intrupt_status) {
    hbt_intrupt_status = 0; // Reset the heartbeat interrupt status
    read_hrt_pulse(); // Read pulse and SpO2 values immediately when a beat is detected
    hbt_sensor_timeout = millis(); // Reset the sensor read timeout to avoid resetting values immediately after a beat is detected
  }else if(millis() > hbt_sensor_timeout + 5000) { // If no beat detected, read pulse and SpO2 values every 2 seconds
    ipm.sta.hrt_value = 0; // Reset heart rate value if no beat detected for a while
    ipm.sta.spo_value = 0; // Reset SpO2 value if no beat detected for a while
  }

  if(millis() > display_timout + 2000){
    //update_display();
    display_timout = millis();
    update_display();
  }
}


