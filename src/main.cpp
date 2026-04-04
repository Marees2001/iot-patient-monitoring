#include <Arduino.h>
#include "controller.h"

PulseOximeter hbt;
DHT dht(TH_SENSOR_Pin, DHTTYPE);

// I2C address (try 0x3C if not worki
TwoWire i2c(MCU_SDA_Pin,MCU_SCL_Pin);

char gsm_rx_buffer[200]; // Buffer for receiving data from GSM module
uint8_t receive_bytes = 0;

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


  Serial.println("I2C Scanner");
  byte error, address;
  for(address = 1; address < 127; address++) {
    i2c.beginTransmission(address);
    error = i2c.endTransmission();
    Serial.printf("I2C scane aadr-%x\n\r",address);
    if (error == 0) {
      Serial.print("Found at 0x");
      Serial.println(address, HEX);
    }
    delay(500);
  }
  Serial.printf("Scan completed...\n\r");

  dht.begin();

  // while(!hbt.begin((PULSEOXIMETER_DEBUGGINGMODE_PULSEDETECT))) {
  //   Serial.println("ERROR: Failed to initialize pulse oximeter");
  //   delay(2000);
  // }
  
  //hbt.setOnBeatDetectedCallback(onBeatDetected);
}

void loop() {
 
  static unsigned long gsm_sync_timeout = 0; // Variable to track GSM synchronization timing
  static unsigned long sensor_read_timeout = 0; // Variable to track sensor reading timing
  static unsigned long display_update_timeout = 0; // Variable to track display update timing
  /*Function for receiving data from GSM module*/
  gsm_receive_data();

  if(millis() - gsm_sync_timeout > 2000) { // Check if 0.5 seconds have passed since the last GSM synchronization
    gsm_sync_timeout = millis(); // Update the last synchronization time
    // Toggle LED state using STM32 HAL
    digitalToggle(STATUS_LED_Pin);      // Push to OLED
    if(ipm.sta.init_gsm != 0xFF) { // Sync GSM every 0.1 seconds
      sync_gsm();
    }else { // If GSM is fully initialized, perform main operations
      // Main operations here (e.g., read sensors, update display, control actuators)
      printf("GSM fully initialized\n\r");
    }
    // Reading temperature or humidity takes about 250 milliseconds!


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
    //read_th_sensor();
    read_hrt_pulse();
  }
}


void read_th_sensor(){
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  ipm.sta.temp_value = dht.readHumidity();
  // Read temperature as Celsius (the default)
  ipm.sta.hum_value = dht.readTemperature();
  Serial.printf("Tempratur :%0.2f\n\r",ipm.sta.temp_value);
  Serial.printf("Humitidy :%0.2f\n\r",ipm.sta.hum_value);
}

void read_hrt_pulse(){
  ipm.sta.hrt_value =  hbt.getHeartRate();
  ipm.sta.spo_value = hbt.getSpO2();
  Serial.printf("Heart rate : %0.2f\n\r",ipm.sta.hrt_value);
  Serial.printf("SPO2  rate : %d %\n\r",ipm.sta.spo_value);
}
