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
//#define OLED_DISPLAY // Uncomment if using an OLED Display

#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <Timezone.h>
#include <ArduinoJson.h>

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

const char* ssid = "**********Add SSID Here*********";
const char* password = "***********Add Password Here*********";

// IFTTT
const char* host = "maker.ifttt.com";
const char* apiKey = "************Add IFTTT API Key Here**********";

#ifdef ADAFRUIT_IO
// **** Adafruit IO Setup ****//
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME  "************Add Adafruit Username Here***********"
#define AIO_KEY "*********Add Adafruit IO Key Here***************"

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
WiFiClient mqtt_client;
Adafruit_MQTT_Client mqtt(&mqtt_client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// Setup garage door feed
Adafruit_MQTT_Publish garage_door = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/garage-door");
// ** Adafruit IO Setup Complete ****//
#endif

const char* month_names[] = {"Jan", "Feb", "Mar", "Apr", "May", "June", "July", "Aug", "Sept", "Oct", "Nov", "Dec"};
const char* day_names[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char* am_pm[] = {"AM", "PM"};
const char* door_open_closed[] = {"Door closed", "Door open"};

//US Pacific Time Zone
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

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
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
               "Content-Type: application/json\r\n" +
               "Content-Length: " + root.measureLength() + "\r\n\r\n" +
               buffer + "\r\n");
  return (1);
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



