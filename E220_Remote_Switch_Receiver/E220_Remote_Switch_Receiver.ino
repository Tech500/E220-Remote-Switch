//E220_Remote_Switch_Receiver.ino
//William Lucid 11/27/2025 @ 11:58 EST

//E220 Module is set to ADDL 2

//Fully connectd schema  AUX connected to ESP32, GPIO15
//Ardino IDE:  ESP32 Board Manager, Version 2.0.17

//  See library downloads for each library license.

//FIXED All

#include "Arduino.h"
#include "LoRa_E220.h"
#include <WiFi.h>
#include <time.h>
#include <FS.h>
#include <LittleFS.h>
#include <FTPServer.h>
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include <INA226_WE.h>
#include <Wire.h>

const char * ssid = "Removed";
const char * password = "Removed";

RTC_DATA_ATTR int activationCount = 0;

FTPServer ftpSrv(LittleFS);

//FTP Credentials
const char * ftpUser = "admin";
const char * ftpPassword = "admin";

//#define AUX_PIN_BITMASK 0x8000

// Persisted RTC variable
RTC_DATA_ATTR int bootCount;
RTC_DATA_ATTR int switch_State = 0;

const int pulseDuration = 300;  // 100 milliseconds (adjust as needed)

#define DESTINATION_ADDL 2
#define FREQUENCY_915
#define CHANNEL 66

#define RXD2 16
#define TXD2 17


#define M0_PIN GPIO_NUM_21
#define M1_PIN GPIO_NUM_19

#define AUX_PIN GPIO_NUM_36
#define TRIGGER 23     //KY002S MOSFET Bi-Stable Switch
#define KY002S_PIN 33  //KY002S MOSFET Bi-Stable Switch --Read output
#define ALERT 4        //INA226 Battery Monitor

#define SDA_PIN 25
#define SCL_PIN 26

int delayTime = 100;  //setmode delay duration

#define I2C_ADDRESS 0x40

INA226_WE ina226 = INA226_WE(I2C_ADDRESS);

volatile bool event = false;

void alert() {
  event = true;
  detachInterrupt(ALERT);
}

volatile bool alertFlag = false;

// Struct to hold date and time components
struct DateTime {
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
};

// Define the maximum length for the timestamp
const int MAX_dateTime_LENGTH = 30;

int data = 0;

int switchState;

struct Message {
  int switchState;
  char dateTime[MAX_dateTime_LENGTH];  // Array to hold date/time string
};

Message message;

void updateTimestamp() {
  String timestamp = get_time();
  timestamp.toCharArray(message.dateTime, MAX_dateTime_LENGTH);
}

#define FPM_SLEEP_MAX_TIME 0xFFFFFFF
void callback() {
  Serial.println("Callback");
  Serial.flush();
}

bool interruptExecuted = false;  // Ensure interruptExecuted is volatile

void IRAM_ATTR wakeUp() {
  // Do not use Serial on interrupt callback
  interruptExecuted = true;
  detachInterrupt(AUX_PIN);
}

void printParameters(struct Configuration configuration);

// ---------- esp32 pins ----------------
LoRa_E220 e220ttl(&Serial2, 36, 21, 19);  //  RX AUX M0 M1

//LoRa_E220 e220ttl(&Serial2, 22, 4, 33, 21, 19, UART_BPS_RATE_9600); //  esp32 RX <-- e220 TX, esp32 TX --> e220 RX AUX_PIN M0 M1
// -------------------------------------

void enterDeepSleep() {
  Serial.println("Preparing for deep sleep...");
  Serial.flush();
  delay(100);  // Allow time for final serial output

  // Set E220 to WOR receiver mode
  e220ttl.setMode(MODE_2_WOR_RECEIVER);
  delay(50);

  // Hold E220 mode pins
  gpio_hold_en(GPIO_NUM_21);  // M0
  gpio_hold_en(GPIO_NUM_19);  // M1
  gpio_deep_sleep_hold_en();

  // Set EXT0 wake on AUX pin (LOW)
  pinMode(GPIO_NUM_36, INPUT);  // external pulldown/pullup required
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_36, LOW);

  Serial.println("Entering deep sleep now...");
  Serial.flush();
  delay(50);

  esp_deep_sleep_start();  // 🔌💤
}


void checkForI2cErrors() {
  byte errorCode = ina226.getI2cErrorCode();
  if (errorCode) {
    Serial.print("I2C error: ");
    Serial.println(errorCode);
    switch (errorCode) {
      case 1:
        Serial.println("Data too long to fit in transmit buffer");
        break;
      case 2:
        Serial.println("Received NACK on transmit of address");
        break;
      case 3:
        Serial.println("Received NACK on transmit of data");
        break;
      case 4:
        Serial.println("Other error");
        break;
      case 5:
        Serial.println("Timeout");
        break;
      default:
        Serial.println("Can't identify the error");
    }
    if (errorCode) {
      while (1) {}
    }
  }
}

void getINA226(const char* dtStamp) {
  float shuntVoltage_mV = 0.0;
  float loadVoltage_V = 0.0;
  float busVoltage_V = 0.0;
  float current_mA = 0.0;
  float power_mW = 0.0;
  ina226.startSingleMeasurement();
  ina226.readAndClearFlags();
  shuntVoltage_mV = ina226.getShuntVoltage_mV();
  busVoltage_V = ina226.getBusVoltage_V();
  current_mA = ina226.getCurrent_mA();
  power_mW = ina226.getBusPower();
  loadVoltage_V = busVoltage_V + (shuntVoltage_mV / 1000);
  checkForI2cErrors();

  Serial.println(dtStamp);
  Serial.print("Shunt Voltage [mV]: ");
  Serial.println(shuntVoltage_mV);
  Serial.print("Bus Voltage [V]: ");
  Serial.println(busVoltage_V);
  Serial.print("Load Voltage [V]: ");
  Serial.println(loadVoltage_V);
  Serial.print("Current[mA]: ");
  Serial.println(current_mA);
  Serial.print("Bus Power [mW]: ");
  Serial.println(power_mW);

  if (!ina226.overflow) {
    Serial.println("Values OK - no overflow");
  } else {
    Serial.println("Overflow! Choose higher current range");
  }
  Serial.println();

  // Open a "log.txt" for appended writing
  File log = LittleFS.open("/log.txt", "a");

  if (!log) {
    Serial.println("file 'log.txt' open failed");
  }

  log.print(dtStamp);
  log.print(" , ");
  log.print(activationCount);
  log.print(" , ");
  log.print(shuntVoltage_mV, 3);
  log.print(" , ");
  log.print(busVoltage_V, 3);
  log.print(" , ");
  log.print(loadVoltage_V, 3);
  log.print(" , ");
  log.print(current_mA, 3);
  log.print(" , ");
  log.print(power_mW, 3);
  log.print(" , ");
  if (alertFlag) {
    log.print("Under Voltage alert");
    alertFlag = false;
  }
  log.println("");
  log.close();
}


void setup() {
  Serial.begin(9600);
  delay(1000);

  wifi_Start();

  bool fsok = LittleFS.begin();
  Serial.printf_P(PSTR("FS init: %s\n"), fsok ? PSTR("ok") : PSTR("fail!"));

  // setup the ftp server with username and password
  // ports are defined in FTPCommon.h, default is
  //   21 for the control connection
  //   50009 for the data connection (passive mode by default)
  ftpSrv.begin(F(ftpUser), F(ftpPassword)); //username, password for ftp.

  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);  // TX = 17, RX = 16
  delay(500);

  pinMode(M0_PIN, OUTPUT);
  pinMode(M1_PIN, OUTPUT);

  e220ttl.setMode(MODE_3_SLEEP);
  delay(delayTime);

  Serial.println("\n\nE220 Remote Switch Receiver\n");

  // Increment boot number and print it every reboot
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));

  // Configure wake-up interrupt on GPIO18 (AUX pin)
  attachInterrupt(GPIO_NUM_27, wakeUp, FALLING);

  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_ON);

  pinMode(AUX_PIN, INPUT_PULLUP);  // GPIO15 WakeUp
  pinMode(TRIGGER, OUTPUT);        // ESP32, GPIO32
  pinMode(KY002S_PIN, INPUT);      //ESP32, GPIO33
  pinMode(ALERT, INPUT);           // ESP32, GPIO4

  int value = digitalRead(KY002S_PIN);  //KY002S, Vo pin
  if (value < 1) {
    digitalWrite(TRIGGER, HIGH);  //KY002S, TRG pin
  }

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!ina226.init()) {
    Serial.println("\nFailed to init INA226. Check your wiring.");
    //while(1){}
  }

  // INA226 configuration
  ina226.enableAlertLatch();
  ina226.setAlertType(BUS_UNDER, 2.9);
  attachInterrupt(digitalPinToInterrupt(ALERT), alert, FALLING);

  e220ttl.begin();
  delay(delayTime);
  e220ttl.setMode(MODE_2_WOR_RECEIVER);
  delay(delayTime);

  // Check if the wakeup was due to external GPIO
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
    //Serial.println("Waked up from external GPIO!");

    gpio_hold_dis(GPIO_NUM_21);
    gpio_hold_dis(GPIO_NUM_19);
    gpio_deep_sleep_hold_dis();

    e220ttl.setMode(MODE_0_NORMAL);
    delay(delayTime);
    e220ttl.sendFixedMessage(0, DESTINATION_ADDL, CHANNEL, "We have waked up from message, but we can't read It!");
  } else {
    e220ttl.setMode(MODE_2_POWER_SAVING);
    delay(delayTime);
    Serial.println("Going to deep sleep!");
    delay(100);

    if (ESP_OK == gpio_hold_en(GPIO_NUM_21)) {
      Serial.println("HOLD 21");
    } else {
      Serial.println("NO HOLD 21");
    }
    if (ESP_OK == gpio_hold_en(GPIO_NUM_19)) {
      Serial.println("HOLD 19");
    } else {
      Serial.println("NO HOLD 19");
    }

    //esp_sleep_enable_ext0_wakeup(GPIO_NUM_27, LOW);

    enterDeepSleep();
  
  }
}

void loop() {

  Message message;  // Declare message outside for full-scope access

  e220ttl.setMode(MODE_2_WOR_RECEIVER);

  if (e220ttl.available() > 0) {
    Serial.println("\nMessage arrived!");

    ResponseStructContainer rsc = e220ttl.receiveMessage(sizeof(Message));
    message = *(Message*)rsc.data;
    Serial.print("Switch State: ");
    Serial.println(message.switchState);
      
    e220ttl.setMode(MODE_0_NORMAL);
    delay(delayTime);
    e220ttl.sendFixedMessage(0, DESTINATION_ADDL, CHANNEL, "We have received the message!");
    delay(10);
    e220ttl.setMode(MODE_3_SLEEP);
    delay(delayTime);

    if (message.switchState == 1) {
      activationCount++;  // Count this as an activation
      updateTimestamp();
      Serial.println(message.dateTime);
      digitalWrite(TRIGGER, LOW);
      delay(100);
      digitalWrite(TRIGGER, HIGH);
      Serial.println("Battery power switched ON");
      //getINA226(message.dateTime);
    }

    if (message.switchState == 2) {
      Serial.println("Battery power switched OFF");
      updateTimestamp();
      Serial.println(message.dateTime);
      digitalWrite(TRIGGER, LOW);
      delay(100);
      digitalWrite(TRIGGER, HIGH);
      enterDeepSleep();
    }

    if (event) {
      Serial.println("Under voltage alert");
      alertFlag = true;
      ina226.readAndClearFlags();
      event = false;
    } 
    rsc.close();   
  }
  

  ftpSrv.handleFTP();    
}

// Function to get the timestamp
String get_time() {
  time_t now;
  time(&now);
  char time_output[MAX_dateTime_LENGTH];
  strftime(time_output, MAX_dateTime_LENGTH, "%a  %d-%m-%y %T", localtime(&now));
  return String(time_output);  // returns timestamp in the specified format
}

void wifi_Start() {

//Server settings
#define ip { 192, 168, 12, 28}
#define subnet \
  { 255, 255, 255, 0 }
#define gateway \
  { 192, 168, 12, 1 }
#define dns \
  { 192, 168, 12, 1 }

  WiFi.mode(WIFI_AP_STA);

  WiFi.begin(ssid, password);
  
  //setting the static addresses in function "wifi_Start
  IPAddress ip;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress dns;

  WiFi.config(ip, gateway, subnet, dns);
}

