//E220_Remote_Switch_Receiver.ino
//William Lucid 7/19/2024 @ 19:41 EST

//E220 Module is set to ADDL 2

//Fully connectd schema  AUX connected to ESP32, GPIO15
//Ardino IDE:  ESP32 Board Manager, Version 2.0.17

//  See library downloads for each library license.

// With FIXED SENDER configuration

#include "Arduino.h"
#include "LoRa_E220.h"
#include <WiFi.h>
#include <time.h>
#include <FS.h>
#include <LittleFS.h>
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include <INA226_WE.h>
#include <Wire.h>

// Persisted RTC variable
RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR int switch_State = 0;

#define AUX_PIN_BITMASK 0x8000

const int pulseDuration = 300;  // 100 milliseconds (adjust as needed)

#define DESTINATION_ADDL 3
#define FREQUENCY_915
#define CHANNEL 66

#define RXD2 16
#define TXD2 17


#define M0_PIN GPIO_NUM_21
#define M1_PIN GPIO_NUM_19

#define AUX_PIN GPIO_NUM_15
#define TRIGGER 23  //KY002S MOSFET Bi-Stable Switch
#define ALERT 4     //INA226 Battery Monitor

#define SDA 13
#define SCL 22

int delayTime = 400;  //setmode delay duration

#define I2C_ADDRESS 0x40

INA226_WE ina226 = INA226_WE(I2C_ADDRESS);

volatile bool event = false;

void alert() {
  event = true;
  detachInterrupt(ALERT);
}

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
LoRa_E220 e220ttl(&Serial2, 15, 21, 19);  //  RX AUX M0 M1

//LoRa_E220 e220ttl(&Serial2, 22, 4, 33, 21, 19, UART_BPS_RATE_9600); //  esp32 RX <-- e220 TX, esp32 TX --> e220 RX AUX_PIN M0 M1
// -------------------------------------

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

void enterSleepMode() {
  // Set M0 and M1 to 1 to enter sleep mode
  //digitalWrite(M0_PIN, HIGH);
  //digitalWrite(M1_PIN, HIGH);
  e220ttl.setMode(MODE_3_SLEEP);
  // Delay to ensure the mode is set
  delay(delayTime);
}

void enterDeepSleep() {
  e220ttl.setMode(MODE_2_POWER_SAVING);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_15, LOW);
  Serial.flush();  // Ensure all Serial data is sent before sleep
  gpio_hold_en(GPIO_NUM_21);
  gpio_hold_en(GPIO_NUM_19);
  gpio_deep_sleep_hold_en();
  Serial.println("Going to deep sleep");
  esp_deep_sleep_start();
  Serial.println("This will never be printed");
}

void setup() {
  Serial.begin(9600);
  delay(1000);

  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2); // TX = 17, RX = 16
  delay(500);

  pinMode(M0_PIN, OUTPUT);
  pinMode(M1_PIN, OUTPUT);

  enterSleepMode();

  Serial.println("\n\nE220 Remote Switch Receiver\n");

  // Increment boot number and print it every reboot
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));

  // Configure wake-up interrupt on GPIO15 (AUX pin)
  attachInterrupt(GPIO_NUM_15, wakeUp, FALLING);

  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_ON);

  pinMode(AUX_PIN, INPUT_PULLUP);  // GPIO15 WakeUp
  pinMode(TRIGGER, OUTPUT);        // ESP32, GPIO23
  pinMode(ALERT, OUTPUT);          // ESP32, GPIO4

  bool fsok = LittleFS.begin(true);
  Serial.printf_P(PSTR("\nFS init: %s\n"), fsok ? PSTR("ok") : PSTR("fail!"));

  Wire.begin(SDA, SCL);

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
    
    gpio_deep_sleep_hold_en();
    delay(1);

    esp_sleep_enable_ext0_wakeup(GPIO_NUM_15, LOW);

    esp_deep_sleep_start();
  }
  //Serial.println("Wake and start listening!");

     
}

void loop() {

  //Serial.println("Test deep sleep");

  e220ttl.setMode(MODE_2_WOR_RECEIVER);
  
  if (e220ttl.available() > 0) {
    Serial.println("\nMessage arrived!");

    ResponseStructContainer rsc = e220ttl.receiveMessage(sizeof(Message));
    if (rsc.status.code == 1) {  // Check if the status is SUCCESS
      Message message = *(Message*)rsc.data;
      //Serial.println(message.switchState);  // This prints to monitor
      //Serial.println(message.dateTime);     // This prints to monitor
      rsc.close();

      e220ttl.setMode(MODE_0_NORMAL);
      delay(delayTime);

      ResponseStatus rsSend = e220ttl.sendFixedMessage(0, DESTINATION_ADDL, CHANNEL, "We have received the message!");
      Serial.println(rsSend.getResponseDescription());
      delay(10);

      e220ttl.setMode(MODE_0_NORMAL);
      delay(delayTime);

      enterSleepMode();
      
      if (message.switchState == 1 ) {
        Serial.println("\nWaked up from external GPIO!");
        Serial.println("Wake and start listening!\n");
        digitalWrite(TRIGGER, HIGH);
        delay(pulseDuration);
        digitalWrite(TRIGGER, LOW);
        switch_State = digitalRead(TRIGGER);
        Serial.println("\nBattery power switched ON");
        Serial.println("ESP32 wake from Deep Sleep\n");
        //getINA226(message.dateTime);
        //enterDeepSleep();
      }
     

      if (message.switchState == 2) {
        digitalWrite(TRIGGER, HIGH);
        delay(pulseDuration);
        digitalWrite(TRIGGER, LOW);
        switch_State = digitalRead(TRIGGER);
        Serial.println("\nBattery power switched OFF");
        Serial.println("ESP32 going to Deep Sleep\n");
        delay(1000);
        enterDeepSleep();
      }

      if (event) {
        digitalWrite(TRIGGER, HIGH);
        delay(pulseDuration);
        digitalWrite(TRIGGER, LOW);
        ina226.readAndClearFlags();
        attachInterrupt(digitalPinToInterrupt(ALERT), alert, FALLING);
        event = false;
        digitalWrite(TRIGGER, LOW);
        ina226.readAndClearFlags();
        //enterDeepSleep();
      }
    }
  }
}
