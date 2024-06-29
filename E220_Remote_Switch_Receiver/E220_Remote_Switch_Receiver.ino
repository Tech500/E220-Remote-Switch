//  E220_Remote_Switch_Receiver.ino
//  06/29/2024  11:17 EST
//  William Lucid

#include "Arduino.h"
#include "LoRa_E220.h"
#include <WiFi.h>
#include <time.h>
#include <FS.h>
#include <LittleFS.h>
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc.h"
#include "driver/rtc_io.h"
#include <INA226_WE.h>
#include <Wire.h>

LoRa_E220 e220ttl(&Serial2, 15, 21, 19);

#define FREQUENCY_868
#define CHANNEL 70  //920.125 Mhz.
#define AUX_PIN_BITMASK 0x8000
#define DESTINATION_ADDL 2

#define AUX_PIN GPIO_NUM_15
#define TRIGGER 23  // KY002S MOSFET Bi-Stable Switch
#define ALERT 4     // INA226 Battery Monitor

#define SDA 13
#define SCL 22

#define RXD2 16
#define TXD2 17

#define I2C_ADDRESS 0x40

RTC_DATA_ATTR int bootCount = 0;

int delayTime = 4000;  //setMode delay
int pulseDuration(100);

int switch_State;  //trigger duration

INA226_WE ina226 = INA226_WE(I2C_ADDRESS);

volatile bool event = false;

void alert() {
  event = true;
  detachInterrupt(ALERT);
}

struct DateTime {
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
};

//char dtStamp[MAX_TIMESTAMP_LENGTH];
const int MAX_TIMESTAMP_LENGTH = 30;

struct switchposition {
  int switchState;
  char timestamp[MAX_TIMESTAMP_LENGTH];
};

switchposition message;

volatile int data = message.switchState;

#define FPM_SLEEP_MAX_TIME 0xFFFFFFF
void callback() {
  Serial.println("Callback");
  Serial.flush();
}

bool interruptExecuted = false;

void IRAM_ATTR wakeUp() {
  interruptExecuted = true;
  //detachInterrupt(AUX_PIN);
}

void printParameters(struct Configuration configuration);

void handleWakeupReason() {
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      Serial.println("Wakeup caused by external signal using RTC_IO");
      break;
    case ESP_SLEEP_WAKEUP_EXT1:
      Serial.println("Wakeup caused by external signal using RTC_CNTL");
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("Wakeup caused by timer");
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      Serial.println("Wakeup caused by touchpad");
      break;
    case ESP_SLEEP_WAKEUP_ULP:
      Serial.println("Wakeup caused by ULP program");
      break;
    default:
      Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
      break;
  }
}

void enterDeepSleep() {
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_ON);
  gpio_hold_en(GPIO_NUM_19);
  gpio_hold_en(GPIO_NUM_21);
  gpio_deep_sleep_hold_en();
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_15, 0);  // Ensure AUX_PIN wakes up ESP32
  Serial.println("Going to sleep now");
  Serial.flush();
  esp_deep_sleep_start();
  Serial.println("This will never be printed");
}

void setup() {
  Serial.begin(9600);
  delay(1000);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  delay(500);

  gpio_deep_sleep_hold_dis();

  struct switchposition message;
  message.switchState = data;
  String timestamp = get_time();
  timestamp.toCharArray(message.timestamp, MAX_TIMESTAMP_LENGTH);  

   delay(delayTime);

  Serial.println("\n\nE220 Remote Switch Receiver\n");

  Wire.begin(SDA, SCL);

  //Increment boot number and print it every reboot
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_15, 0); 

  pinMode(AUX_PIN, INPUT_PULLUP);
  pinMode(TRIGGER, OUTPUT);  // ESP32, GPIO23
  pinMode(ALERT, OUTPUT);    // ESP32, GPIO4

  attachInterrupt(GPIO_NUM_15, wakeUp, LOW);

  bool fsok = LittleFS.begin(true);
  Serial.printf_P(PSTR("\nFS init: %s\n"), fsok ? PSTR("ok") : PSTR("fail!"));

  e220ttl.begin();
 
  handleWakeupReason();

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  if (wakeup_reason == esp_sleep_get_wakeup_cause()) {
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

    Serial.println();
    Serial.println("Start sleep!");
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

    esp_sleep_enable_ext0_wakeup(GPIO_NUM_15,LOW);

    gpio_deep_sleep_hold_en();
    //Serial.println("Going to sleep now");
    delay(1);  
   }
   
    //e220ttl.setMode(MODE_0_NORMAL);
    //delay(delayTime);

    Serial.println();

    Serial.println("Wake and start listening!");

    enterDeepSleep(); // Explicitly call deep sleep  
 
}

// The loop function is called in an endless loop
void loop() {

  if (e220ttl.available() > 1) {
    Serial.println("\nMessage arrived!");
    ResponseStructContainer rsc = e220ttl.receiveMessage(sizeof(switchposition));
    switchposition message = *(switchposition*)rsc.data;

    Serial.println(message.switchState); 
    Serial.println(message.timestamp);
    free(rsc.data);
    
    // Work only with full connection
    e220ttl.setMode(MODE_0_NORMAL);
    delay(delayTime);

    ResponseStatus rsSend = e220ttl.sendFixedMessage(0, DESTINATION_ADDL, 70, "We have received the message!");
    // Check If there is some problem of succesfully send
    Serial.println(rsSend.getResponseDescription());
  }

  if(interruptExecuted) {
    Serial.flush();
    interruptExecuted = false;

    Serial.println("WakeUp Callback, AUX pin go LOW and start receive message!\n");

    //------------------------- Task execution ------------------------------

    if (message.switchState == 1) {
      digitalWrite(TRIGGER, HIGH);
      delay(pulseDuration);
      digitalWrite(TRIGGER, LOW);
      switch_State = digitalRead(TRIGGER);  // Read current switch state
      Serial.println("\nBattery power switched ON");
      Serial.println("ESP32 wake from Deep Sleep\n");
      //getINA226(dtStamp);
      char dtStamp[MAX_TIMESTAMP_LENGTH];
      strncpy(dtStamp, message.timestamp, MAX_TIMESTAMP_LENGTH);  // Copy timestamp
      //logBattery(dtStamp); // Pass temporary copy
      enterDeepSleep();
    }

    if (message.switchState == 2) {
      digitalWrite(TRIGGER, HIGH);
      delay(pulseDuration);
      digitalWrite(TRIGGER, LOW);
      switch_State = digitalRead(TRIGGER);  // Read current switch state
      Serial.println("\nBattery power switched OFF");
      Serial.println("ESP32 going to Deep Sleep\n");
      delay(1000);
      enterDeepSleep();
    }

    if (event) {
      digitalWrite(TRIGGER, HIGH);
      delay(pulseDuration);
      digitalWrite(TRIGGER, LOW);
      ina226.readAndClearFlags();  // reads interrupt and overflow flags and deletes them
      //getINA226(dtStamp);  //displayResults();
      attachInterrupt(digitalPinToInterrupt(ALERT), alert, FALLING);
      event = false;
      digitalWrite(TRIGGER, LOW);
      ina226.readAndClearFlags();
      enterDeepSleep();
    }
  }  
}


int main() {
  // Create an instance of the Message struct
  switchposition message;

  // Get the timestamp using the get_time function and assign it to the struct member
  String timestamp = get_time();
  timestamp.toCharArray(message.timestamp, MAX_TIMESTAMP_LENGTH);

  // Now you can use message.timestamp as needed...

  return 0;
}

// Function to get the timestamp
String get_time() {
  time_t now;
  time(&now);
  char time_output[MAX_TIMESTAMP_LENGTH];
  strftime(time_output, MAX_TIMESTAMP_LENGTH, "%a  %d-%m-%y %T", localtime(&now));
  return String(time_output);  // returns timestamp in the specified format
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
  log.print(shuntVoltage_mV, 3);
  log.print(" , ");
  log.print(busVoltage_V, 3);
  log.print(" , ");
  log.print(loadVoltage_V, 3);
  log.print(" , ");
  log.print(current_mA, 3);
  log.print(" , ");
  log.print(power_mW, 3);
  log.println("");
  log.close();
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
      //while (1) {}
    }
  }
}

void printParameters(struct Configuration configuration) {
  Serial.println("----------------------------------------");

  Serial.print(F("HEAD : "));
  Serial.print(configuration.COMMAND, HEX);
  Serial.print(" ");
  Serial.print(configuration.STARTING_ADDRESS, HEX);
  Serial.print(" ");
  Serial.println(configuration.LENGHT, HEX);
  Serial.println(F(" "));
  Serial.print(F("AddH : "));
  Serial.println(configuration.ADDH, HEX);
  Serial.print(F("AddL : "));
  Serial.println(configuration.ADDL, HEX);
  Serial.println(F(" "));
  Serial.print(F("Chan : "));
  Serial.print(configuration.CHAN, DEC);
  Serial.print(" -> ");
  Serial.println(configuration.getChannelDescription());
  Serial.println(F(" "));
  Serial.print(F("SpeedParityBit     : "));
  Serial.print(configuration.SPED.uartParity, BIN);
  Serial.print(" -> ");
  Serial.println(configuration.SPED.getUARTParityDescription());
  Serial.print(F("SpeedUARTDatte     : "));
  Serial.print(configuration.SPED.uartBaudRate, BIN);
  Serial.print(" -> ");
  Serial.println(configuration.SPED.getUARTBaudRateDescription());
  Serial.print(F("SpeedAirDataRate   : "));
  Serial.print(configuration.SPED.airDataRate, BIN);
  Serial.print(" -> ");
  Serial.println(configuration.SPED.getAirDataRateDescription());
  Serial.println(F(" "));
  Serial.print(F("OptionSubPacketSett: "));
  Serial.print(configuration.OPTION.subPacketSetting, BIN);
  Serial.print(" -> ");
  Serial.println(configuration.OPTION.getSubPacketSetting());
  Serial.print(F("OptionTranPower    : "));
  Serial.print(configuration.OPTION.transmissionPower, BIN);
  Serial.print(" -> ");
  Serial.println(configuration.OPTION.getTransmissionPowerDescription());
  Serial.print(F("OptionRSSIAmbientNo: "));
  Serial.print(configuration.OPTION.RSSIAmbientNoise, BIN);
  Serial.print(" -> ");
  Serial.println(configuration.OPTION.getRSSIAmbientNoiseEnable());
  Serial.println(F(" "));
  Serial.print(F("TransModeWORPeriod : "));
  Serial.print(configuration.TRANSMISSION_MODE.WORPeriod, BIN);
  Serial.print(" -> ");
  Serial.println(configuration.TRANSMISSION_MODE.getWORPeriodByParamsDescription());
  Serial.print(F("TransModeEnableLBT : "));
  Serial.print(configuration.TRANSMISSION_MODE.enableLBT, BIN);
  Serial.print(" -> ");
  Serial.println(configuration.TRANSMISSION_MODE.getLBTEnableByteDescription());
  Serial.print(F("TransModeEnableRSSI: "));
  Serial.print(configuration.TRANSMISSION_MODE.enableRSSI, BIN);
  Serial.print(" -> ");
  Serial.println(configuration.TRANSMISSION_MODE.getRSSIEnableByteDescription());
  Serial.print(F("TransModeFixedTrans: "));
  Serial.print(configuration.TRANSMISSION_MODE.fixedTransmission, BIN);
  Serial.print(" -> ");
  Serial.println(configuration.TRANSMISSION_MODE.getFixedTransmissionDescription());


  Serial.println("----------------------------------------");
}

void printModuleInformation(struct ModuleInformation moduleInformation) {
  Serial.println("----------------------------------------");
  Serial.print(F("HEAD: "));
  Serial.print(moduleInformation.COMMAND, HEX);
  Serial.print(" ");
  Serial.print(moduleInformation.STARTING_ADDRESS, HEX);
  Serial.print(" ");
  Serial.println(moduleInformation.LENGHT, DEC);

  Serial.print(F("Model no.: "));
  Serial.println(moduleInformation.model, HEX);
  Serial.print(F("Version  : "));
  Serial.println(moduleInformation.version, HEX);
  Serial.print(F("Features : "));
  Serial.println(moduleInformation.features, HEX);
  Serial.println("----------------------------------------");
}
