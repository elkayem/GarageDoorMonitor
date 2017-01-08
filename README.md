# GarageDoorMonitor

![Monitor](/images/IMG_1274.JPG)

The Garage Door Monitor continously monitors whether your garage door is closed, and sends out an email or text message if it has been left open for more than 15 minutes.  It uses the popular ESP8266 WiFi module, and relays messages to you via the IFTTT web service. In our household, we have accidentally left the garage door open on multiple occasions, and this device has saved our bacon more than once.  

Features:
* Email and/or text sent every 15 minutes when your door has been left open, notifying you what time it was opened and how long
* Email/text sent once your door has been closed
* Synchronization with NIST time
* Enable/disable button, which can temporarily disable the monitor
* RGB LED indicating monitor status
* (Optional) logging of door status via Adafruit IO

The following parts are required, as pictured below:
* NodeMCU V1.0 development board (sometimes also listed as v2 at online shops such as eBay or AliExpress)
* Normally Open magnetic switch
* (2) Female headers
* Common Anode RGB LED
* Screw Terminal Block (2 pin 3.5mm Pitch)
* 6x6mm tactile button
* (3) 300 Ohm resistors
* (Optional) 4-pin male header
* (Optional) Printed Circuit Board (Eagle files included)

The PCB is indicated as optional, since the circuit is simple enough to hand solder on a PCB breadboard if desired (schematic shown below).  The 4-pin male header is used to bring out the I2C pins for future upgrades.  The I2C port is currently unused, but the code could easily be upgraded to include an I2C display or additional sensor.  

<img src="/images/IMG_1271.JPG" alt="Monitor" width="400" height="300"> <img src="/images/IMG_1273.JPG" alt="Monitor" width="400" height="300">
<img src="/images/Schematic.JPG" alt="Schematic" width="600" height="450"> 

# Instructions
1. Add SSID and password to the sketch GarageDoorMonitor.ino after "ssid =" and "password =".  
2. Set up an account at IFTTT.com.  Add the "Maker" service to your account.  Copy the Maker service API Key, which can be found under the Maker service settings.  It is a long character sequence following at the end of the URL in Settings, i.e., the character sequence appearing at API_KEY in https://maker.ifttt.com/use/API_KEY.  Note: once the key is created, do not press the "Edit connection" link, or it will create a new key and remove the old one.
3. Add the IFTTT API_KEY to the sketch after "apiKey =".
4. (Optional) If logging is desired, uncomment #define ADAFRUIT_IO and set up and account at https://io.adafruit.com/.  Add AIO_USERNAME and AIO_KEY to sketch.  Add a feed called "garage-door" to your Adafruit IO account.
5. The code is set up for Pacific Timezone.  Change as necessary.
6. Compile sketch using the Arduino IDE, and upload to the NodeMCU.  A number of libraries will also need to get added: NTPClient, ESP8266WiFi, WiFi, Time, Timezone, ArduinoJson, and Adafruit_MQTT.  Most of these libraries are added through the Library Manager (Sketch > Include Library > Manage Libraries).  The only exception is Timezone, which must be manually installed from https://github.com/JChristensen/Timezone.  Also, there are plenty of tutorials on the web on how to set up the Arduino IDE for the NodeMCU board, so those instructions won't be repeated here.
7. In IFTTT, create a new applet.  Choose "Maker" as the trigger (the "if this" part), and call the event "door_open".   Choose Gmail as the action (the "then that" part).  Place the desired emails on the To line (text messages can also be sent depending on the provider, e.g., as xxxxxxxxxx@txt.att.net), and fill out the Subject and Body as shown below, where "Value1" and "Value2" are added as "Ingredients".

  ![door_open](/images/IFTTT1.JPG)
8. Create another applet in IFTTT with "Maker" as the trigger and Gmail as the action.  Call the Maker event name "door_closed".  Put in the desired email(s) for the Gmail action and fill out the Subject and Body as shown below:

  ![door_closed](/images/IFTTT2.JPG)
9. (Optional) You may optionally create an additional applet which will send out an email at midnight every day giving you confirmation that your door monitor is still working, and the last time the garage door was closed.  (If this feature is not desired, the related part of the sketch can be removed.)  This applet also has "Maker" as the trigger and Gmail as the action. Call the Maker event name "door_monitor_active".  Put in the desired email(s) for the Gmail action and fill out the Subject and Body as shown below:

  ![door_monitor_active](/images/IFTTT3.JPG)
10. Solder together your garage door monitor, install it in your garage, and plug it in.  You should be good to go!  

The LED will be red at turn-on, and will change to green once it has connected to WiFi and synced with NIST time.  When the garage door is shut, the light will remain green.  If open, it will change to magenta for 15 minutes, and will then change to red after it has sent out its first email.  Pressing the tactile button will cause the light to blink blue, and the monitor will not send any emails for 2 hours, or whenever the button is pushed again, which ever comes first.  Any failure to connect to WiFi will cause the monitor to blink red until it is able to reconnect.

I have left an I2C connector on the PCB for possible future upgrades.  Some suggested upgrades:
* Make the SSID, password, and API Key configurable.  This can be done using the WiFiManager library.  When the device is first turned on, it will create a WiFi access point.  The user can then log into the access point and add SSID, password, and API Key.  These items get stored in the device's nonvolatile memory, and will continue to present after power cycling the device.  It may be useful to have a way to reset the configurable data, for example, by pressing the tactile button for 10 seconds.  
* Add an I2C OLED display to show current status, time, etc. 
* Add other I2C sensors, such as temperature.  Multiple I2C devices can be added in parallel.
* Design a 3D printed case
