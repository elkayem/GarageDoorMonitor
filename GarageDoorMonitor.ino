#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <Timezone.h>
#include <ArduinoJson.h>

//#define ADAFRUIT_IO  // Uncomment if using Adafruit IO account
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

const char* month_names[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
const char* day_names[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char* am_pm[] = {"AM", "PM"};
const char* door_open_closed[] = {"Door closed", "Door open"};

//US Pacific Time Zone
TimeChangeRule myDST = {"PDT", Second, Sun, Mar, 2, -420};    //Daylight time = UTC - 7 hours
TimeChangeRule mySTD = {"PST", First, Sun, Nov, 2, -480};     //Standard time = UTC - 8 hours
Timezone myTZ(myDST, mySTD);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "time.nist.gov", 0, 7200000); // Update time every two hours

int doorStatus, doorStatusPrev = CLOSED, openMessage = false;
unsigned long int changeTime, changeTimer, lastCheckTime, standbyModeTime;
char changeTimeStr[50];
int dailyMessageSent;

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

void setup() {
  Serial.begin(115200);

  pinMode(STANDBY, INPUT_PULLUP);
  pinMode(DOOR, INPUT);  // GPIO16 does not have a pull-up resistor.  See work-around in readDoorStatus
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);

  status_leds(1, 0, 0, 0); // Solid red at turn-on

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected");

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
  recordTime(changeTimeStr);
  dailyMessageSent = day();

  status_leds(0, 1, 0, 0); // Set-up complete, set to green
}

void loop()
{
  blink_leds();
  readStandbyButton();
  if (standbyMode) {
    if (millis() - standbyModeTime > STANDBY_TIMEOUT)
      standbyMode = 0;  // Automatically go out of stanby mode after timeout period
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
    recordTime(changeTimeStr);
    changeTime = millis();
    changeTimer = changeTime;
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

void recordTime(char *date_time_str) {
  char tod[10];
  TimeChangeRule *tcr;

  timeClient.update();

  setTime(myTZ.toLocal(timeClient.getEpochTime(), &tcr));

  formattedTime(tod, hourFormat12(), minute(), second());

  sprintf(date_time_str, "%s %s %s on %s, %s %d, %d", tod, am_pm[isPM()],
          tcr -> abbrev, day_names[weekday() - 1], month_names[month() - 1], day(), year());

}
#define colonDigit(digit) digit < 10 ? ":0" : ":"
void formattedTime(char *tod, int hours, int minutes, int seconds)
{
  // sprintf(tod, "%d%s%d%s%d", hours, colonDigit(minutes), minutes, colonDigit(seconds), seconds);  // Hours, minutes, seconds
  sprintf(tod, "%d%s%d", hours, colonDigit(minutes), minutes);  // Only report hours and minutes
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
    return (0);
  }

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
          if (digitalRead(DOOR) == OPEN)
            status_leds(0, 1, 0, 0); // Door shut, status = Green
          else
            status_leds(1, 0, 1, 0); // Door open, status = Magenta
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

