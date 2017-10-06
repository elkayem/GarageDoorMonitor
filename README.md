# GarageDoorMonitor

![Monitor](/images/IMG_1485.JPG)

The Garage Door Monitor continously monitors whether your garage door is closed, and sends out an email or text message if it has been left open for more than 15 minutes.  It uses the popular ESP8266 WiFi module, and relays messages to you via the IFTTT web service. In our household, we have accidentally left the garage door open on multiple occasions, and this device has saved our bacon more than once.  

Features:
* Email and/or text sent every 15 minutes when your door has been left open, notifying you what time it was opened and how long
* Email/text sent once your door has been closed
* Synchronization with NIST time
* Enable/disable button, which can temporarily disable the monitor
* RGB LED indicating monitor status
* (Optional) logging of door status via Adafruit IO
* (Optional) OLED Display

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
* (Optional) 128x64 I2C OLED Display  (Preferably a two-color display, e.g., yellow/blue)

The PCB is indicated as optional, since the circuit is simple enough to hand solder on a PCB breadboard if desired (schematic shown below).  An OLED display as shown below can enhance the monitor, but is not necessary. There are also two different variations of 3D printed cases provided, one which is used when there is no OLED display, and the other to be used with an OLED.  The non-OLED case was designed in OpenSCAD, and the OLED case (pictured below) was designed in Sketchup 2017.

<img src="/images/IMG_1271.JPG" alt="Monitor" width="400" height="300"> <img src="/images/IMG_1273.JPG" alt="Monitor" width="400" height="300">
<img src="/images/IMG_1274.JPG" alt="MonotorAssembled" width="400" height="300">  <img src="/images/IMG_1329.JPG" alt="OLED" width="400" height="300"> 
<img src="/images/IMG_1336.JPG" alt="OLED_Case" width="400" height="300">  <img src="/images/CaseSketchup.JPG" alt="CaseDesign" width="400"> 
<img src="/images/Schematic.JPG" alt="Schematic" width="600" height="450"> 


# Instructions
1. Configure Arduino IDE
  1. Install Arduino IDE 1.8.1 (or later) from arduino.cc
  2. Open File>Preferences, and enter the following URL into "Additional Board Manager URLs": http://arduino.esp8266.com/stable/package_esp8266com_index.json
  3. In the Tools menu, configure Board: NodeMCU 1.0 (ESP-12E Module), CPU Frequency: 80 MHz, Upload Speed: 115200.
2. Add SSID and password to the sketch GarageDoorMonitor.ino after "ssid =" and "password =".  
3. Set up an account at IFTTT.com.  Add the "Webhook" service to your account.  Copy the Webhook service API Key, which can be found under the Webhook service settings.  It is a long character sequence following at the end of the URL in Settings, i.e., the character sequence appearing at API_KEY in https://maker.ifttt.com/use/API_KEY.  Note: once the key is created, do not press the "Edit connection" link, or it will create a new key and remove the old one.
4. Add the IFTTT API_KEY to the sketch after "apiKey =".
5. (Optional) If logging is desired, uncomment #define ADAFRUIT_IO and set up and account at https://io.adafruit.com/.  Add AIO_USERNAME and AIO_KEY to sketch.  Add a feed called "garage-door" to your Adafruit IO account.
6. The code is set up for Pacific Timezone.  Change as necessary.
7. Install the following libraries: NTPClient, ESP8266WiFi, WiFi, Time, Timezone, ArduinoJson, and Adafruit_MQTT.  Most of these libraries are added through the Library Manager (Sketch > Include Library > Manage Libraries).  The only exception is Timezone, which must be manually installed from https://github.com/JChristensen/Timezone.  
8. (Optional) If using an OLED display, uncomment the line #define OLED_DISPLAY in the sketch.  Install the libraries Adafruit_GFX and Adafruit_SSD1306.  In the file Adafruit_SSD1306.h (found in the folder Arduino\libraries\Adafruit_SSD1306, where 
"Arduino" is your top level Arduino directory), uncomment the line #define SSD1306_128_64, and comment out all other displays. **Important Note:** If you don't do the above step, the library will assume you are using a 32-pixel display and the displayed text will not fit on the screen.
9. Connect the NodeMCU board to your computer using a micro USB cable, and set Tools>Port to the new port that appears.  Your computer should automatically install the driver, but if it does not, you may need to manually download and install the CP2102 driver from http://www.silabs.com/products/mcu/pages/usbtouartbridgevcpdrivers.aspx.  
10. Press the Upload button to compile the sketch and upload to the NodeMCU.  The most common reason for failing to compile are an selecting the wrong board or not installing all the required libraries.
11. In IFTTT, create a new applet.  You should see a sentence that says "if +this then that".  Press "+this", and choose the "Webhook" service.  Select the "Receive a web request" trigger.  Call the event "door_open", and create the trigger. Next, press "+that" (from the "if this then that" sentence) and choose the Gmail service.  Select the "Send and email" action.    Choose Gmail as the action (the "then that" part).  Place the desired emails on the To line (text messages can also be sent depending on the provider, e.g., as xxxxxxxxxx@txt.att.net).  Fill out the Subject as: Garage Door Open for  "{{Value1}}" Minutes.  Fill out the body as: The Garage Door was opened at {{Value1}} and has been open for {{Value2}} minutes.  Press the "Create action" buttong.  It should look as shown below:

  ![door_open](/images/IFTTT1.JPG)
12. Create another applet in IFTTT with Webhook as the trigger and Gmail as the action.  Call the Webhook event name "door_closed".  Put in the desired email(s) for the Gmail action and fill out the Subject and Body as shown below:

  ![door_closed](/images/IFTTT2.JPG)
13. (Optional) You may optionally create an additional applet which will send out an email at midnight every day giving you confirmation that your door monitor is still working, and the last time the garage door was closed.  (If this feature is not desired, the related part of the sketch can be removed.)  This applet also has Webhook as the trigger and Gmail as the action. Call the Webhook event name "door_monitor_active".  Put in the desired email(s) for the Gmail action and fill out the Subject and Body as shown below:

  ![door_monitor_active](/images/IFTTT3.JPG)
14. Solder together your garage door monitor, install it in your garage, and plug it in.  If using an OLED display, connect display to the I2C headers using 4" female-female jumper wires, taking care to match the 3V3 and GND on the display to the board.    
  * **Important Note:** Take care to solder the RGB LED pins in the correct order.  The longest pin is the common anode, which should be soldered in the hole second from the left, where the NodeMCU is on the left side of the board.  The LED case also has a flat edge, which should match the flat edge shown on the board silkscreen.
  
15. Print out a case, and install in your garage.  Two case designs are provided in the "cases" folder. 

The LED will be red at turn-on, and will change to green once it has connected to WiFi and synced with NIST time.  When the garage door is shut, the light will remain green.  If open, it will change to magenta for 15 minutes, and will then change to red after it has sent out its first email.  Pressing the tactile button will cause the light to blink blue, and the monitor will not send any emails for 2 hours, or whenever the button is pushed again, which ever comes first.  Any failure to connect to WiFi will cause the monitor to blink red until it is able to reconnect.

I have left an I2C connector on the PCB for possible future upgrades.  Some suggested upgrades:
* Make the SSID, password, and API Key configurable.  This can be done using the WiFiManager library.  When the device is first turned on, it will create a WiFi access point.  The user can then log into the access point and add SSID, password, and API Key.  These items get stored in the device's nonvolatile memory, and will continue to present after power cycling the device.  It may be useful to have a way to reset the configurable data, for example, by pressing the tactile button for 10 seconds.  
* Add other I2C sensors, such as temperature.  Multiple I2C devices can be added in parallel.
* Design a new 3D printed case, or improve upon one of the cases provided.

# Door Monitor Discussion
This project was designed for the LM SSC Innovation Garage.  If you are in the process of building one and have some improvements/ideas to share, or a little troubleshooting help, please join the discussion at Gitter:
[![Join the chat at https://gitter.im/GarageDoorMonitor/Lobby](https://badges.gitter.im/GarageDoorMonitor/Lobby.svg)](https://gitter.im/GarageDoorMonitor/Lobby?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

