/*    
 *    GarageDoorMonitor
 *	  Copyright (C) 2017  Larry McGovern
 *	
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License <http://www.gnu.org/licenses/> for more details.
 */

//#define ADAFRUIT_IO  // Uncomment if using Adafruit IO account
#define OLED_DISPLAY // Comment out if not using an OLED Display

#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <WiFiManager.h>
#include <TimeLib.h>
#include <Timezone.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

#ifdef OLED_DISPLAY
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#endif

#ifdef ADAFRUIT_IO
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#endif

#define D0 16 // LED_BUILTIN
#define D1 5 // I2C Bus SCL (clock)
#define D2 4 // I2C Bus SDA (data)
#define D3 0 // 
#define D4 2 //  Blue LED 
#define D5 14 // SPI Bus SCK (clock)
#define D6 12 // SPI Bus MISO 
#define D7 13 // SPI Bus MOSI
#define D8 15 // SPI Bus SS (CS) 
#define D9 3 // RX0 (Serial console) 
#define D10 1 // TX0 (Serial console) 

#define OPEN HIGH  //  For normally open read switch, OPEN = HIGH.  Swap if using normally closed switch
#define CLOSED LOW

// Timezone and DST EEPROM addresses
char tz_str[4], dst_str[2];
#define TZ_ADDR  0
#define TZ_LENGTH 4
#define DST_ADDR 4
#define DST_LENGTH 2

// IFTTT
const char* host = "maker.ifttt.com";
#define API_KEY_LENGTH 50
#define API_KEY_ADDR TZ_LENGTH + DST_LENGTH  // First six bytes store time zone info
char apiKey[API_KEY_LENGTH];

#ifdef ADAFRUIT_IO  // **** Adafruit IO Setup ****//
  #define AIO_SERVER      "io.adafruit.com"
  #define AIO_SERVERPORT  1883
  #define AIO_USERNAME_LENGTH 50
  #define AIO_KEY_LENGTH 50
  #define AIO_USERNAME_ADDR API_KEY_ADDR + API_KEY_LENGTH
  #define AIO_KEY_ADDR AIO_USERNAME_ADDR + AIO_USERNAME_LENGTH

  char aioUsername[AIO_USERNAME_LENGTH], aioKey[AIO_KEY_LENGTH], aioFeed[AIO_USERNAME_LENGTH+20];  

  // Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
  WiFiClient mqtt_client;
  Adafruit_MQTT_Client mqtt(&mqtt_client, AIO_SERVER, AIO_SERVERPORT, aioUsername, aioKey);

  // Setup garage door feed
  Adafruit_MQTT_Publish garage_door = Adafruit_MQTT_Publish(&mqtt, aioUsername);
  
  #define EEPROM_LENGTH TZ_LENGTH + DST_LENGTH + API_KEY_LENGTH + AIO_USERNAME_LENGTH + AIO_KEY_LENGTH
#else
  #define EEPROM_LENGTH TZ_LENGTH + DST_LENGTH + API_KEY_LENGTH
#endif

const char* month_names[] = {"Jan", "Feb", "Mar", "Apr", "May", "June", "July", "Aug", "Sept", "Oct", "Nov", "Dec"};
const char* day_names[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char* am_pm[] = {"AM", "PM"};
const char* door_open_closed[] = {"Door closed", "Door open"};

// US Pacific Time Zone -- Time zone is configurable in EEPROM
TimeChangeRule myDST = {"PDT", Second, Sun, Mar, 2, -420};    //Daylight time = UTC - 7 hours
TimeChangeRule mySTD = {"PST", First, Sun, Nov, 2, -480};     //Standard time = UTC - 8 hours
Timezone myTZ(myDST, mySTD);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "time.nist.gov", 0, 7200000); // Update time every two hours

int wifiStatus, doorStatus, doorStatusPrev = CLOSED, openMessage = false;
unsigned long int changeTime, changeTimer, lastCheckTime, standbyModeTime;
char changeTimeStr[50], closeTimeStr[20];
int dailyMessageSent;
int prevSecond;

bool redState, greenState, blueState, blinkState, standbyMode = 0;

#define DOOR       D0
#define STANDBY    D4
#define RED_LED    D5
#define GREEN_LED  D6
#define BLUE_LED   D7

#define TIMER_MS 900000 // Time to send out alert, in ms.  900000 = 15 minutes
#define BLINK_PERIOD 500
#define DOOR_CHECK_PERIOD 100   // Check door status every 100 msec
#define STANDBY_TIMEOUT 7200000  // Automatically go out of standby mode after 2 hours

#ifdef OLED_DISPLAY
#define OLED_RESET  D8
Adafruit_SSD1306 display(OLED_RESET);
#endif

void setup() {
  int i;
  
  Serial.begin(115200);

  pinMode(STANDBY, INPUT_PULLUP);
  pinMode(DOOR, INPUT);  // GPIO16 does not have a pull-up resistor.  See work-around in readDoorStatus
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);
  
#ifdef OLED_DISPLAY
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // OLED I2C Address, may need to change for different device,
                                              // Check with I2C_Scanner
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Connecting...");
  display.display();
#endif
  
  status_leds(1, 0, 0, 0); // Solid red at turn-on

  
  EEPROM.begin(EEPROM_LENGTH);

  // Read info from EEPROM
  eepromRead(tz_str, TZ_ADDR, TZ_LENGTH);
  eepromRead(dst_str, DST_ADDR, DST_LENGTH);
  eepromRead(apiKey, API_KEY_ADDR, API_KEY_LENGTH);
  
  #ifdef ADAFRUIT_IO
    eepromRead(aioUsername, AIO_USERNAME_ADDR, AIO_USERNAME_LENGTH);
    eepromRead(aioKey, AIO_KEY_ADDR, AIO_KEY_LENGTH);
  #endif
    
  // Setup WiFiManager

  WiFiManager MyWifiManager;
  MyWifiManager.setAPCallback(configModeCallback);
  
  WiFiManagerParameter custom_api_text("<br/><br/><br/>IFTTT API Key:<br/>");
  MyWifiManager.addParameter(&custom_api_text);
  
  WiFiManagerParameter custom_api_key("apikey", "API Key", apiKey, API_KEY_LENGTH);
  MyWifiManager.addParameter(&custom_api_key);

  WiFiManagerParameter custom_timezone_text("<br/><br/><br/>Standard Time Zone Offset (US Pacific = -8, US Eastern = -5, GMT = 0) :<br/>");
  MyWifiManager.addParameter(&custom_timezone_text);
  
  WiFiManagerParameter custom_timezone("timezone", "Time Zone", tz_str, TZ_LENGTH);
  MyWifiManager.addParameter(&custom_timezone);

  WiFiManagerParameter custom_dst_text("<br/><br/><br/>Enable US Daylight Savings Time rules? (Y/N):<br/>");
  MyWifiManager.addParameter(&custom_dst_text);
  
  WiFiManagerParameter custom_dst("dst", "DST", dst_str, DST_LENGTH);
  MyWifiManager.addParameter(&custom_dst);

  #ifdef ADAFRUIT_IO
    WiFiManagerParameter custom_aio_username_text("<br/><br/><br/>Adafruit IO Username:<br/>");
    MyWifiManager.addParameter(&custom_aio_username_text);
  
    WiFiManagerParameter custom_aio_username("aio_username", "Adafruit IO Username", aioUsername, AIO_USERNAME_LENGTH);
    MyWifiManager.addParameter(&custom_aio_username);

    WiFiManagerParameter custom_aio_key_text("<br/><br/><br/>Adafruit IO Key:<br/>");
    MyWifiManager.addParameter(&custom_aio_key_text);
  
    WiFiManagerParameter custom_aio_key("aio_key", "Adafruit IO Key", aioKey, AIO_KEY_LENGTH);
    MyWifiManager.addParameter(&custom_aio_key);
  #endif

  MyWifiManager.autoConnect("GARAGE_DOOR");


  bool saveEEPROM = false;
  if (strcmp(apiKey, custom_api_key.getValue()) || strcmp(tz_str, custom_timezone.getValue()) || strcmp(dst_str, custom_dst.getValue())) {  // If data changed, need to save to EEPROM
    saveEEPROM = true;
  }
  #ifdef ADAFRUIT_IO
    if (strcmp(aioKey, custom_aio_key.getValue()) || strcmp(aioUsername, custom_aio_username.getValue())) {  
      saveEEPROM = true;
    }  
  #endif

  if (saveEEPROM) {
    strcpy(tz_str, custom_timezone.getValue());
    for (i=0; i<TZ_LENGTH; i++) {
      EEPROM.write(TZ_ADDR+i, tz_str[i]);
    }
    
    strcpy(dst_str, custom_dst.getValue());
    for (i=0; i<DST_LENGTH; i++) {
      EEPROM.write(DST_ADDR+i, dst_str[i]);
    }
    
    strcpy(apiKey, custom_api_key.getValue());
    for (i=0; i<API_KEY_LENGTH; i++) {
      EEPROM.write(API_KEY_ADDR+i, apiKey[i]);
    }
    
    #ifdef ADAFRUIT_IO
      strcpy(aioUsername, custom_aio_username.getValue());
      for (i=0; i<AIO_USERNAME_LENGTH; i++) {
        EEPROM.write(AIO_USERNAME_ADDR+i, aioUsername[i]);
      }  
      
      strcpy(aioKey, custom_aio_key.getValue());
      for (i=0; i<AIO_KEY_LENGTH; i++) {
        EEPROM.write(AIO_KEY_ADDR+i, aioKey[i]);
      }   
    #endif
    
    EEPROM.commit();
  }

  // Update timezone info
  int tz = atoi(tz_str);
  if (tz < -24 || tz > 24) tz = 0; 
  mySTD.offset = tz*60;

  if (dst_str[0] == 'y' || dst_str[0] == 'Y') {
    myDST.offset = mySTD.offset + 60;
    switch (tz) {
      case -8:
        strcpy(mySTD.abbrev, "PST");
        strcpy(myDST.abbrev, "PDT");
        break;
      case -7:
        strcpy(mySTD.abbrev, "MST");
        strcpy(myDST.abbrev, "MDT");
        break;        
      case -6:
        strcpy(mySTD.abbrev, "CST");
        strcpy(myDST.abbrev, "CDT");
        break; 
      case -5:
        strcpy(mySTD.abbrev, "EST");
        strcpy(myDST.abbrev, "EDT");
        break;    
      default:   
        strcpy(mySTD.abbrev, "STD");
        strcpy(myDST.abbrev, "DST");  
    }
  }
  else {
    myDST.offset = mySTD.offset;
      strcpy(mySTD.abbrev, "");
      strcpy(myDST.abbrev, "");
  }
  myTZ = Timezone(myDST, mySTD);

#ifdef ADAFRUIT_IO
  mqtt = Adafruit_MQTT_Client(&mqtt_client, AIO_SERVER, AIO_SERVERPORT, aioUsername, aioKey);
  strcpy(aioFeed,aioUsername); // Add feed
  strcat(aioFeed, "/feeds/garage-door"); // Add feed
  garage_door = Adafruit_MQTT_Publish(&mqtt, aioFeed);
#endif
    
  wifiStatus = 1;
  Serial.println(" Connected");

#ifdef OLED_DISPLAY  
  display.clearDisplay();
  display.setCursor(0,0);
  display.print("Wifi Connected");
  display.setCursor(0,28);
  display.println("Updating local time");
  display.display();
#endif

  timeClient.begin();
  while (!timeClient.update()) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Local time updated.");

  changeTime = millis();
  changeTimer = changeTime;
  lastCheckTime = millis() - DOOR_CHECK_PERIOD;
  standbyModeTime = millis();
  updateTime(1);
  dailyMessageSent = day();
  
  status_leds(0, 1, 0, 0); // Set-up complete, set to green
}

void loop()
{
  if (second() != prevSecond) {
    updateTime(0);  // Update time displayed on OLED
    prevSecond = second();
  }
  blink_leds();
  readStandbyButton();
  if (standbyMode) {
    if (millis() - standbyModeTime > STANDBY_TIMEOUT)
      standbyMode = 0;  // Automatically go out of standby mode after timeout period
    else
      return;
  }
  if (millis() - lastCheckTime < DOOR_CHECK_PERIOD) return; 
  
  doorStatus = readDoorStatus();
  
  if (doorStatus != doorStatusPrev) { // Door has opened or closed
    
    delay(500);  doorStatus = readDoorStatus();  // Resample after 500 msec, to filter out glitches

    if (doorStatus == doorStatusPrev) return;     // Return if change in status was a glitch
    
    if (!blinkState) { // Change LED to indicate door open/shut only if LED not blinking
      if (doorStatus == CLOSED)
        status_leds(0, 1, 0, 0); // Door shut, status = Green
      else
        status_leds(1, 0, 1, 0); // Door open, status = Magenta
    } 

    changeTime = millis();
    changeTimer = changeTime;
    updateTime(1);
    Serial.println(String(door_open_closed[(int)(doorStatus==OPEN)]) + " at " + changeTimeStr);
    sendAdaIO();
    doorStatusPrev = doorStatus;   
  }

  if ( (doorStatus == OPEN) && (millis() - changeTimer > TIMER_MS)) { // Door has been open for at least 15 minutes
    if (!sendIFTTT("/trigger/door_open/with/key/")) return; // Send door open message, return if failed
    changeTimer = millis();
    openMessage = true;
    status_leds(1, 0, 0, 0); // Turn LED red if door open for > 15 min
  }
  else if ( (openMessage == true) && (doorStatus == CLOSED)) { // Open door message has gone out, but door is closed now
    if (!sendIFTTT("/trigger/door_closed/with/key/")) return;  // Send door closed message, return if failed
    openMessage = false;
    status_leds(0, 1, 0, 0); // Turn LED green once door has closed again
  }
  else if (day() != dailyMessageSent) { // It's a new day
    if (!sendIFTTT("/trigger/door_monitor_active/with/key/")) return;  // Send an "I'm Healthy" message, return if failed
    dailyMessageSent = day();
    status_leds(0, 1, 0, 0); // Turn green, system is healthy
  }
  
  lastCheckTime = millis();
}

#define colonDigit(digit) digit < 10 ? ":0" : ":"

void updateTime(int updateChangeTime) {  // If updateChangeTime == 1, the global changeTimeString is updated
  char tod_hm[10],tod_hms[15], date_str[25], Timer[10];
  int timerMin, timerSec;
  TimeChangeRule *tcr;

  timeClient.update();

  setTime(myTZ.toLocal(timeClient.getEpochTime(), &tcr));

  sprintf(tod_hm, "%d%s%d", hourFormat12(), colonDigit(minute()), minute());  // Hours, minutes
  sprintf(tod_hms, "%s%s%d", tod_hm, colonDigit(second()), second());  // Hours, minutes, seconds

  if (updateChangeTime) {  
    sprintf(changeTimeStr, "%s %s %s on %s, %s %d, %d", tod_hm,  am_pm[isPM()],
          tcr -> abbrev, day_names[weekday() - 1], month_names[month() - 1], day(), year());
    if (doorStatus == CLOSED) {
      sprintf(closeTimeStr, "%s %s, %s %d", tod_hm,  am_pm[isPM()], month_names[month() - 1], day());  
    }
  }

#ifdef OLED_DISPLAY
  sprintf(date_str, "%s, %s %d", day_names[weekday() - 1], month_names[month() - 1], day());
  display.clearDisplay();
  display.setCursor(0,8);
  display.setFont(&FreeMonoBold12pt7b);
  display.println(tod_hms);
  display.setCursor(0,23);
  display.setFont(); 
  display.println(date_str);
  display.println("");
  display.println("Door last closed at:"); 
  display.println(closeTimeStr);

  if (!wifiStatus) {
    display.println("Error: No Internet");
  }
  else if (standbyMode) {
    timerSec = (STANDBY_TIMEOUT - (millis() - standbyModeTime)) / 1000;
    timerMin = timerSec / 60;
    timerSec -= 60*timerMin;    
    sprintf(Timer,"%d%s%d",timerMin,colonDigit(timerSec),timerSec);
    display.println("");
    display.print("Standby Mode: ");
    display.print(Timer); 
  }
  else if (doorStatus == OPEN) {
    timerSec = (millis() - changeTime) / 1000;
    timerMin = timerSec / 60;
    timerSec -= 60*timerMin;
    sprintf(Timer,"%d%s%d",timerMin,colonDigit(timerSec),timerSec);
    display.println("");
    display.print("Door open for: ");
    display.print(Timer); 
  }
  display.display();
#endif
}


int sendAdaIO()
{
#ifdef ADAFRUIT_IO
  if (!mqtt.connected()) {  // Reconnect if not connected
     Serial.print("Connecting to MQTT... ");

     uint8_t retries = 3;
     int8_t ret;
     while ((ret = mqtt.connect()) != 0) { // Try to connect three times
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(1000);  // wait 1 second1
       retries--;
       if (retries == 0) {
         return(0);
       }
    }
    Serial.println("MQTT Connected!");
  }
  
  Serial.println("Sending to Adafruit.io");

  garage_door.publish(doorStatus==CLOSED);  // Previous door state
  delay(1000);
  garage_door.publish(doorStatus==OPEN);    // Current door state

  Serial.println("Sent");
#endif
  return(1);
}


int sendIFTTT(String url)
{
  static int ifttt_retry = 0;
  
  Serial.print("connecting to ");
  Serial.println(host);
  char buffer[256];

  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    status_leds(1, 0, 0, 1); // Blink red, unable to connect to IFTTT
    wifiStatus = 0;
    return (0);
  }
  wifiStatus = 1;

  StaticJsonBuffer<200> jsonBuffer;

  JsonObject& root = jsonBuffer.createObject();
  root["value1"] = changeTimeStr;
  root["value2"] = (millis() - changeTime) / 60000;

  root.printTo(buffer, sizeof(buffer));
  Serial.println("JSON Data:");
  Serial.println(buffer);

  url += apiKey;
  Serial.print("Requesting URL: ");
  Serial.println(url);

  client.print(String("POST ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
			         "Connection: close\r\n" +
               "Content-Type: application/json\r\n" +
               "Content-Length: " + root.measureLength() + "\r\n\r\n" +
               buffer + "\r\n");
  
  while(client.connected()) {
    if(client.available()) {
      String line = client.readStringUntil('\r');
      Serial.print(line);
      if (line.length() > 2) { // IFFT returns a message when it receives a signal
        ifttt_retry = 0;
        return(1);
      }
    } else {
      delay(50);
    }             
  }

  if (ifttt_retry > 2) {  // Try a maximum of 3 times
    ifttt_retry = 0;
    return(1);
  }
  ifttt_retry++;
  return (0);  // Returning zero will cause this function to be called again
}

void status_leds(bool red, bool green, bool blue, bool blink)
{
  redState   = !red;
  greenState = !green;
  blueState  = !blue;
  blinkState = blink;

  digitalWrite(RED_LED, redState);
  digitalWrite(GREEN_LED, greenState);
  digitalWrite(BLUE_LED, blueState);
}

void blink_leds()
{
  static bool blink_on_off = 0;
  static unsigned long int blinkTimer = millis();

  if (blinkState) {
    if (millis() - blinkTimer > BLINK_PERIOD) {
      blink_on_off = !blink_on_off;
      blinkTimer = millis();

      digitalWrite(RED_LED, blink_on_off || redState);
      digitalWrite(GREEN_LED, blink_on_off || greenState);
      digitalWrite(BLUE_LED, blink_on_off || blueState);
    }
  }
}


void readStandbyButton() {
  static int buttonState = HIGH, lastButtonState = HIGH;
  static long lastDebounceTime = 0;

  int buttonReading = digitalRead(STANDBY);

  if (buttonReading != lastButtonState) {
    lastDebounceTime = millis();  // reset debounce timer
  }

  if ((millis() - lastDebounceTime) > 50) { // Wait 50 msec before reading button

    if (buttonReading != buttonState) {
      buttonState = buttonReading;

      if (buttonState == LOW) {  // Button has been pushed (grounded)
        standbyMode = !standbyMode;  // Toggle standby mode
        standbyModeTime = millis();  // Reset timer
        if (standbyMode) {
          status_leds(0, 0, 1, 1); // Blink blue in standby mode
          Serial.println("Standby Mode");
        }
        else {
          if (readDoorStatus() == CLOSED) {
            status_leds(0, 1, 0, 0); // Door shut, status = Green
          }
          else {
            status_leds(1, 0, 1, 0); // Door open, status = Magenta
            changeTimer = millis();  // Reset 15 minute timer
          }
          Serial.println("Ready Mode");
        }
      }
    }
  }

  if (((millis() - lastDebounceTime) > 5000) && (buttonReading == LOW)) {  // Button has been depressed for 5 seconds
     WiFiManager MyWifiManager;
     MyWifiManager.resetSettings();  // Reset WIFI
     delay(1000);
     ESP.restart();
  }
  lastButtonState = buttonReading;
}

int readDoorStatus() {
  // Note: on NodeMCU modules with a blue LED, once D0 (GPIO16) is connected to ground and the blue LED is lit,
  // it will stay dimly lit after the D0 circuit is opened again, with GPIO pulled to 0.9V.  The consequence
  // is that digitalRead(DOOR) will continue to return LOW.  On NodeMCU modules with a red LED, the LED forward 
  // voltage is small enough to pull up GPIO16 (D0) enough for digitalRead(DOOR) to read HIGH when the switch is open.
  // The following four lines are a workaround for units with a blue LED     
  pinMode(DOOR,OUTPUT);
  digitalWrite(DOOR,HIGH);
  delay(1);
  pinMode(DOOR,INPUT); 
  
  return(digitalRead(DOOR)); 
}

void configModeCallback (WiFiManager *myWiFiManager) {
#ifdef OLED_DISPLAY 
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("GARAGE DOOR MONITOR");
  display.println("WIFI SETUP");
  display.setCursor(0,23);
  display.println("To configure Wifi,  ");
  display.println("connect to Wifi ");
  display.println("network GARAGE_DOOR");
  display.println("and open 192.168.4.1");
  display.println("in web browser");
  display.display();   
#endif
}

void eepromRead(char *str, int addr, int len) {
  for (int i=0; i<len; i++) {
    str[i] = EEPROM.read(addr+i);
  }
  str[len-1] = '\0'; // Ensures last byte will be null character
}

