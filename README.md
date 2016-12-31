# GarageDoorMonitor

![Monitor](/images/IMG_1274.JPG)

The Garage Door Monitor continously monitors whether your garage door is closed, and sends out an email or text message if it has been left open for more than 15 minutes.  It uses the popular ESP8266 WiFi module, and relays messages to you via the IFTTT web service. 

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

The PCB is indicated as optional, since the circuit is simple enough to hand solder on a PCB breadboard if desired (schematic shown below).  TheThe 4-pin male header is used to bring out the I2C pins for future upgrades.  The I2C port is currently unused, but the code could easily be upgraded to include an I2C display or additional sensor.  

<img src="/images/IMG_1271.JPG" alt="Monitor" width="400" height="300"> <img src="/images/IMG_1273.JPG" alt="Monitor" width="400" height="300">
<img src="/images/Schematic.JPG" alt="Schematic" width="600" height="450"> 

# Instructions
1) Add SSID and password to code.  
2) Set up an account at IFTTT.com.  Add the "Maker" service to your account.  Copy the Maker service API Key, which can be found under the Maker service settings.  It is a long character sequence following at the end of the URL in Settings, i.e., the character sequence appearing at API_KEY in https://maker.ifttt.com/use/API_KEY.



If logging is desired, uncomment #define ADAFRUIT_IO and set up and account at adafruit.io.  
