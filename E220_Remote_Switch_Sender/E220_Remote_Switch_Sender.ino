//  E220_Remote_Switch_Sender.ino  
//  06/27/2024  13:22 EST
//  William Lucid

 /*  Renzo Mischianti <https://mischianti.org>
 *
 * https://mischianti.org
 *
 * E220          ----- esp32         
 * M0            ----- 19 (or 3.3v)  
 * M1            ----- 21 (or GND)   
 * TX            ----- TX2 (PullUP)  
 * RX            ----- RX2 (PullUP)  
 * AUX           ----- 15  (PullUP)   //Pin 15 is what is given in the Lora e220 constructor.
 * VCC           ----- 3.3v/5v       
 * GND           ----- GND           
 *
 *  Connections confirmed DMM continuity tests.
 */

#include <Arduino.h>
#include "WiFi.h"
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <time.h>
#include "LoRa_E220.h"
#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include <esp_sleep.h>
#include <Ticker.h>

#import "index7.h"  // Video feed HTML; do not remove

#define DESTINATION_ADDL 3
#define FREQENCY_868
#define CHANNEL 70  //920.125 Mhz

WiFiClient client;
boolean connected = false;
WiFiUDP udp;
const int udpPort = 1337;
char incomingPacket[255];
const char *udpAddress1 = "pool.ntp.org";
const char *udpAddress2 = "time.nist.gov";
#define TZ "EST+5EDT,M3.2.0/2,M11.1.0/2"

LoRa_E220 e220ttl(&Serial2, 15, 21, 19); // RX AUX M0 M1

#define AUX_PIN GPIO_NUM_15
#define RXD2 16
#define TXD2 17

int delayTime = 1000;  //setMode delay

const char *ssid = "XXXX";
const char *password = "XXXXXXXX";

AsyncWebServer server(80);

struct DateTime {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
};

const int MAX_TIMESTAMP_LENGTH = 30;

struct switchposition{
  int switchState;
  char timestamp[MAX_TIMESTAMP_LENGTH];
};

switchposition message;

volatile int data = message.switchState;

Ticker oneTick;
Ticker onceTick;

String linkAddress = "xxx.xxx.xxx.xxx:80";

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
volatile int watchdogCounter = 0;
int cameraPowerOff = 0;
int cameraFlag;
int needAnotherCountdown = 0;
bool got_interrupt = false;

void ISRwatchdog() {
    portENTER_CRITICAL_ISR(&mux);
    watchdogCounter++;
    portEXIT_CRITICAL_ISR(&mux);
}

void ISRcamera() {
    batteryOff();
}

void interruptHandler() {
    got_interrupt = true;
}

void setup() {
    Serial.begin(9600);
    delay(500);
    Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
    delay(500);
    wifi_Start();

    struct switchposition message;
    message.switchState = data;
    String timestamp = get_time();
    timestamp.toCharArray(message.timestamp, MAX_TIMESTAMP_LENGTH);  

    pinMode(AUX_PIN, INPUT_PULLUP);
    attachInterrupt(AUX_PIN, interruptHandler, FALLING);

    e220ttl.begin();
    e220ttl.setMode(MODE_1_WOR_TRANSMITTER);
    delay(1000);

    Serial.println("\nHi, I'm going to send WOR message!");
    ResponseStatus rs = e220ttl.sendFixedMessage(0, DESTINATION_ADDL, CHANNEL, "Hello, world? WOR!");
    Serial.println(rs.getResponseDescription());

    Serial.println("\nWebserver and E220-900T30D Remote Switch\n");

    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    setenv("TZ", TZ, 1);
    tzset();

  server.on("/relay", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, PSTR("text/html"), HTML7, processor7);
    data = 1;
    needAnotherCountdown = 1;
    countdownTrigger();
  });

    server.begin();
    oneTick.attach(1.0, ISRwatchdog);
}

void loop() {
  DateTime currentDateTime = getCurrentDateTime();

  if ((currentDateTime.minute % 15 == 0) && (currentDateTime.second == 0)) {
      // webInterface(); // Uncomment if needed
  }


// If something available
  if (e220ttl.available()>1) {
	  // read the String message
	ResponseContainer rc = e220ttl.receiveMessage();
	// Is something goes wrong print error
	if (rc.status.code!=1){
		Serial.println(rc.status.getResponseDescription());
	}else{
		// Print the data received
		Serial.println(rc.status.getResponseDescription());
		Serial.println(rc.data);
	}
  }
  if (Serial.available()) {
	String input = Serial.readString();
	ResponseStatus rs = e220ttl.sendFixedMessage(0, DESTINATION_ADDL, 70, input);
	// Check If there is some problem of succesfully send
	Serial.println(rs.getResponseDescription());
  }
}

String processor7(const String &var) {
  if (var == F("LINK"))
    return linkAddress;
    return String();
}

void batteryOff() {
  int data = 2;
  switchOne(data);
  onceTick.detach();
}

void countdownTrigger() {
  // Perform countdown actions here
  Serial.println("\nCountdown timer triggered!\n");
  //getDateTime();
  // Schedule the next countdown if needed
  if (needAnotherCountdown == 1) {
    onceTick.once(60, ISRcamera);
    int data = 1;
    switchOne(data);
    needAnotherCountdown = 0;
  }
}

DateTime getCurrentDateTime() {
    DateTime currentDateTime;
    time_t now = time(nullptr);
    struct tm *ti = localtime(&now);
    currentDateTime.year = ti->tm_year + 1900;
    currentDateTime.month = ti->tm_mon + 1;
    currentDateTime.day = ti->tm_mday;
    currentDateTime.hour = ti->tm_hour;
    currentDateTime.minute = ti->tm_min;
    currentDateTime.second = ti->tm_sec;
    return currentDateTime;
}

String get_time() {
    time_t now;
    time(&now);
    char time_output[MAX_TIMESTAMP_LENGTH];
    strftime(time_output, MAX_TIMESTAMP_LENGTH, "%a %m/%d/%y %T", localtime(&now));
    return String(time_output);
}

void switchOne(int data) {

  if (data == 1) {
    data = 1;
    Serial.println("\nBattery Switch is ON");
    Serial.println("ESP32 waking from Deep Sleep\n");
  }

  if (data == 2) {
    data = 2;
    Serial.println("\nBattery power switched OFF");
    Serial.println("ESP32 going to Deep Sleep\n");
  }
  
  get_time(); 

  Serial.print("data:  "); Serial.println(data);
  
  e220ttl.setMode(MODE_1_WOR_TRANSMITTER);
  delay(100);
  
  switchposition message;
 
  //initialize struct members
  data = message.switchState; 
  // Initialize the timestamp 
  String timestamp = get_time();
  timestamp.toCharArray(message.timestamp, MAX_TIMESTAMP_LENGTH);  

  Serial.print("Time_Stamp:  "); Serial.println(message.timestamp);

  // Send message
	ResponseStatus rs = e220ttl.sendFixedMessage(0, DESTINATION_ADDL, CHANNEL, &message, sizeof(switchposition));
	// Check If there is some problem of succesfully send
  Serial.println(rs.getResponseDescription());

  data = 0;

} 

void wifi_Start() {
    WiFi.mode(WIFI_AP_STA);
    Serial.print("\n\nConnecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);

    IPAddress ip(10, 0, 0, 27);
    IPAddress gateway(10, 0, 0, 1);
    IPAddress subnet(255, 255, 255, 0);
    IPAddress dns(10, 0, 0, 1);
    WiFi.config(ip, gateway, subnet, dns);

    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
        delay(1000);
    }

    Serial.print("Server IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Port: 80");
    Serial.print("MAC: ");
    Serial.println(WiFi.macAddress());
    Serial.print("Wi-Fi Channel: ");
    Serial.println(WiFi.channel());
    Serial.printf("Connection result: %d\n", WiFi.waitForConnectResult());
    server.begin();
}
