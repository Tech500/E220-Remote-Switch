
//E220_Remote_Switch_Sender.ino
//William Lucid 07/19/2024 @ 19:41 EST

//E220 Module is set to ADDL 3

//Fully connectd schema  AUX connected to ESP32, GPIO15
//Ardino IDE:  ESP32 Board Manager, Version 2.0.17

//  See library downloads for each library license.

// With FIXED SENDER configuration

#include <Arduino.h>
#include "WiFi.h"
#include <WiFiUdp.h> 
#include <HTTPClient.h>
#include <time.h>
#include "LoRa_E220.h"
#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include <Ticker.h>

#import "index7.h"  //Video feed HTML; do not remove

#define DESTINATION_ADDL 2
#define FREQENCY_915
#define CHANNEL 66

#define RXD2 16
#define TXD2 17

#define AUX_PIN GPIO_NUM_15

int delayTime = 100;  //setmode delay duration

WiFiClient client;

///Are we currently connected?
boolean connected = false;

WiFiUDP udp;
// local port to listen for UDP packets
const int udpPort = 1337;
char incomingPacket[255];
char replyPacket[] = "Hi there! Got the message :-)";
//NTP Time Servers
const char * udpAddress1 = "pool.ntp.org";
const char * udpAddress2 = "time.nist.gov";

#define TZ "EST+5EDT,M3.2.0/2,M11.1.0/2"

int DOW, MONTH, DATE, YEAR, HOUR, MINUTE, SECOND;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

char strftime_buf[64];

// ---------- esp32 pins --------------
 LoRa_E220 e220ttl(&Serial2, 15, 21, 19); //  RX AUX M0 M1

//LoRa_E220 e220ttl(&Serial2, 22, 4, 33, 21, 19, UART_BPS_RATE_9600); //  esp32 RX <-- e220 TX, esp32 TX --> e220 RX AUX_PIN M0 M1
// -------------------------------------

// Replace with your network details
const char *ssid = "R2D2";
const char *password = "sissy4357";

AsyncWebServer server(80);

int data = 0;

// Struct to hold date and time components
struct DateTime {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
};

// Define the maximum length for the dateTime
const int MAX_dateTime_LENGTH = 30;

int switchState;

struct Message {
  int switchState;
  char dateTime[MAX_dateTime_LENGTH];  // Array to hold date/time string
};

Message message;

Ticker oneTick;
Ticker onceTick;

String linkAddress = "xxx.xxx.xxx.xxx:80";

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

volatile int watchdogCounter;
int totalwatchdogCounter;
int cameraPowerOff = 0;
int watchDog;

void ISRwatchdog() {

  portENTER_CRITICAL_ISR(&mux);

  watchdogCounter++;

  if (watchdogCounter >= 75) {

    watchDog = 1;
  }

  portEXIT_CRITICAL_ISR(&mux);
}

int cameraFlag;
int needAnotherCountdown = 0;

void ISRcamera() {
  batteryOff();
}

bool got_interrupt = false;
 
void interruptHandler() {
  got_interrupt = true;
}  

void sendWOR(){
  e220ttl.setMode(MODE_1_WOR_TRANSMITTER);
  delay(delayTime);
  // Send message
  ResponseStatus rs = e220ttl.sendFixedMessage(0, DESTINATION_ADDL, CHANNEL, "Hello, world? WOR!");
  e220ttl.setMode(MODE_0_NORMAL);
  delay(delayTime);
}

void setup() {
  Serial.begin(9600);
  delay(500);

  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  
  Message message;
  message.switchState = data;
  String dateTimeStr = get_time();
  if (!dateTimeStr.isEmpty()) {
    strncpy(message.dateTime, dateTimeStr.c_str(), MAX_dateTime_LENGTH - 1);
    message.dateTime[MAX_dateTime_LENGTH - 1] = '\0'; // Ensure null-termination
  }

  wifi_Start(); 
  
  pinMode(AUX_PIN, INPUT);

  attachInterrupt(GPIO_NUM_15, interruptHandler, FALLING);
  
  // Startup all pins and UART
  e220ttl.begin();
  delay(delayTime);
  e220ttl.setMode(MODE_1_WOR_TRANSMITTER);
  delay(delayTime);

  //sendWOR();
 
  e220ttl.setMode(MODE_0_NORMAL);
  delay(delayTime);

  Serial.println("\n\n\nWebserver and");
  Serial.println("E220-900T30D Remote Switch\n");

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  // See https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv for Timezone codes for your region
  setenv("TZ", "EST+5EDT,M3.2.0/2,M11.1.0/2", 3);   // this sets TZ to Indianapolis, Indiana


  server.on("/relay", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, PSTR("text/html"), HTML7, processor7);
    sendWOR();
    data = 1;
    needAnotherCountdown = 1;
    countdownTrigger();
  });

  server.begin();

  oneTick.attach(1.0, ISRwatchdog);  //watchdog  ISR triggers every 1 second
} 

void loop() {

  DateTime currentDateTime = getCurrentDateTime();	
  
  if((currentDateTime.minute % 15 == 0) && (currentDateTime.second == 0)){
    //webInterface();  //Sends URL Get request to wake up Radio and ESP32 at 1 minute interval
                      // URL = http://10.0.0.27/relay	 
    //delay(1000);
  }
  
  //udp only send data when connected
  if (connected)
  {

    //Send a packet
    udp.beginPacket(udpAddress1, udpPort);
    udp.printf("Seconds since boot: %u", millis() / 1000);
    udp.endPacket();
  }

  // If something available
  if (e220ttl.available() > 1) {
    // read the String message
    ResponseContainer rc = e220ttl.receiveMessage();
    // Is something goes wrong print error
    if (rc.status.code != 1) {
      Serial.println(rc.status.getResponseDescription());
    } else {
      // Print the data received
      Serial.println(rc.status.getResponseDescription());
      Serial.println(rc.data);
    }
  }
}
      
String processor7(const String &var) {

  //index7.h

  if (var == F("LINK"))
    return linkAddress;

  return String();
}

void batteryOff() {
  int data = 2;
  switchOne(data);
  oneTick.detach();
}

void configTime()
{

  configTime(0, 0, udpAddress1, udpAddress2);
  setenv("TZ", "EST+5EDT,M3.2.0/2,M11.1.0/2", 3);   // this sets TZ to Indianapolis, Indiana
  tzset();

  //udp only send data when connected
  if (connected)
  {

    //Send a packet
    udp.beginPacket(udpAddress1, udpPort);
    udp.printf("Seconds since boot: %u", millis() / 1000);
    udp.endPacket();
  }

  Serial.print("wait for first valid dateTime");

  while (time(nullptr) < 100000ul)
  {
    Serial.print(".");
    delay(5000);
  }

  Serial.println("\nSystem Time set\n");

  get_time();

  Serial.println(message.dateTime);

}

void countdownTrigger() {
  // Perform countdown actions here
  Serial.println("\nCountdown timer triggered!\n");
  //getDateTime();
  // Schedule the next countdown if needed
  if (needAnotherCountdown == 1) {
    onceTick.once(60, ISRcamera);
    data = 1;
    switchOne(data);
    needAnotherCountdown = 0;
  }
}

// Function to get current date and time
DateTime getCurrentDateTime() {
    DateTime currentDateTime;
    time_t now = time(nullptr);
    struct tm *ti = localtime(&now);

    // Extract individual components
    currentDateTime.year = ti->tm_year + 1900;
    currentDateTime.month = ti->tm_mon + 1;
    currentDateTime.day = ti->tm_mday;
    currentDateTime.hour = ti->tm_hour;
    currentDateTime.minute = ti->tm_min;
    currentDateTime.second = ti->tm_sec;

    return currentDateTime;
}

// Function to get the dateTime
String get_time() {

    time_t now;
    time(&now);
    char time_output[MAX_dateTime_LENGTH];
    strftime(time_output, MAX_dateTime_LENGTH, "%a  %m/%d/%y   %T", localtime(&now)); 
    return String(time_output); // returns dateTime in the specified format
}

void switchOne(int data) {
  if (data == 1) {
    Serial.println("\nWaked up from external GPIO!");
    Serial.println("Wake and start listening!\n");
    delay(500);
    Serial.println("\nESP32 waking from Deep Sleep");  
    Serial.println("Battery Switch is ON\n"); 
  }

  if (data == 2) {
    Serial.println("\nBattery power switched OFF");
    Serial.println("ESP32 going to Deep Sleep\n");
  }

  Serial.println("Hi, I'm going to send message!");

  e220ttl.setMode(MODE_1_WOR_TRANSMITTER);
  
  delay(delayTime);
  
  get_time();

  Message message; 
  
  //initialize struct members
  message.switchState = data;
  
  // Initialize the dateTime 
  String dateTimeStr = get_time();
  if (!dateTimeStr.isEmpty()) {
    strncpy(message.dateTime, dateTimeStr.c_str(), MAX_dateTime_LENGTH - 1);
    message.dateTime[MAX_dateTime_LENGTH - 1] = '\0'; // Ensure null-termination
  }

  Serial.print("switchState:  "); Serial.println(message.switchState);
  Serial.print("dateTime:  "); Serial.println(message.dateTime);

  // Send message
  ResponseStatus rs = e220ttl.sendFixedMessage(0, DESTINATION_ADDL, CHANNEL, &message, sizeof(Message));
  // Check If there is some problem of succesfully send
    Serial.println(rs.getResponseDescription());  
}  


int sendMessage(int data){
  Serial.println("Hi, I'm going to send message!");

  e220ttl.setMode(MODE_1_WOR_TRANSMITTER);
  
  delay(delayTime);
  
  get_time();

  Message message; 

  //Initialize struct members
  message.switchState = data;
 
  // Initialize the dateTime 
  String dateTimeStr = get_time();
  if (!dateTimeStr.isEmpty()) {
    strncpy(message.dateTime, dateTimeStr.c_str(), MAX_dateTime_LENGTH - 1);
    message.dateTime[MAX_dateTime_LENGTH - 1] = '\0'; // Ensure null-termination
  }

  //Serial.print("switchState:  "); Serial.println(message.switchState);
  //Serial.print("dateTime:  "); Serial.println(message.dateTime);

  // Send message
  ResponseStatus rs = e220ttl.sendFixedMessage(0, DESTINATION_ADDL, CHANNEL, &message, sizeof(Message));
  // Check If there is some problem of succesfully send
  Serial.println(rs.getResponseDescription());  
}

void webInterface() {

  //getTimeDate();

  String data = "http://10.0.0.27/relay";

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;    // Declare object of class HTTPClient

    http.begin(data);  // Specify request destination

    // No need to add content-type header for a simple GET request

    int httpCode = http.GET();   // Send the GET request

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();  // Get the response payload

      Serial.print("HttpCode: ");
      Serial.print(httpCode);   // Print HTTP return code
      Serial.println("\n");
      //Serial.print("  Data echoed back from Hosted website: ");
      //Serial.println(payload);  // Print payload response      

      http.end();  // Close HTTPClient
    } else {
      Serial.print("HttpCode: ");
      Serial.print(httpCode);   // Print HTTP return code
      Serial.println("  URL Request failed.");

      http.end();   // Close HTTPClient
    }
  } else {
    Serial.println("Error in WiFi connection");
  }
}

void wifi_Start() {

//Server settings
#define ip { 10, 0, 0, 27}
#define subnet \
  { 255, 255, 255, 0 }
#define gateway \
  { 10, 0, 0, 1 }
#define dns \
  { 10, 0, 0, 1 }

  WiFi.mode(WIFI_AP_STA);

  Serial.println();
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());

  // We start by connecting to WiFi Station
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  delay(1000);

  //setting the static addresses in function "wifi_Start
  IPAddress ip;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress dns;

  WiFi.config(ip, gateway, subnet, dns);

  Serial.println("Web server running. Waiting for the ESP32 IP...");

  // Printing the ESP IP address
  Serial.print("Server IP:  ");
  Serial.println(WiFi.localIP());
  Serial.print("Port:  ");
  Serial.println("80");
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.print("Wi-Fi Channel: ");
  Serial.println(WiFi.channel());
  Serial.println("\n");

  delay(300);

  WiFi.waitForConnectResult();

  Serial.printf("Connection result: %d\n", WiFi.waitForConnectResult());

  server.begin();


  if (WiFi.waitForConnectResult() != 3) {
    delay(3000);
    wifi_Start();
  }
}
