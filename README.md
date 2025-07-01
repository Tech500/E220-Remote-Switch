Updated 07/01/2025 --Now included; KY002S Bi-Stable MOSFET switch and INA226 Battery Monitor

Updated 07/01/2025  --Now with E220 Sleep Mode; reducing always on current to microamp range!

[E220-Remote-Switch Demo video with sleep mode current monitoring](https://www.youtube.com/watch?v=_cuUFP5C3NI)

Demonstration mode:
1.	ESP32 Receiver; push receiver reset button; this puts the ESP32 into deep sleep.
2.	Open browser to "http://192.168.12.27/relay"; this will create a web request for turning on battery power. and start a countdown timer to turn off battery power, then put the ESP32 receiver into deep sleep.
3.	First message is the WOR message to wake the E220 Receiver module and the Deep Sleeping ESP32.  Second message from E220 Sender is required to turn on battery power and start the countdown timer. Both messages are sent when server request arrives from a click on web link to view video camera.  
Video:  E220-Remote-Switch Demo with sleep current monitoring

Three advantages of using the Ebyte, E220-900T30D are increased distance 10 km estimated, at power of 30 dbm and Sleep current of 5 uA.  E220-900T30D third feature is the ability to send a WOR message to wake up the receiving transceiver allowing second message; to turn on battery power.
Transmit current of 620 mA is almost instantaneous at 30dbm to send; up to 200 bytes, before dropping current. Receiving current; for a message, 17.2 mA. Current values are from Ebytes E220-900T30D User Manual.  Measured Standby current 11.8 mA.  E220 module is always drawing current; utilizing E220 Sleep Mode, Sleep current ranges from .54 uA to 97.33 uA the complete cycle of messages, measured with digital multimeter set for microamps.  Occasional very brief, on order of a few milliseconds; meter over loads occurring due to message being transmitted or received.  

I would like to express my gratitude to ChatGPT, an AI developed by OpenAI, for providing valuable assistance and guidance 
throughout the development of my E220 LoRa module project. The insights and support received were instrumental in overcoming 
technical challenges and optimizing the project's performance.

Thank you Renzo Mischianti for your LoRa E220 Library (xReef), E220 Ebyte E220 articles, Lora Ebyte module Community support, and the Ebyte E220 Support page.

Wolfgang Ewald thank you; learned much from your article "Using LoRa with the EByte E220, E22 and E32 series".  Thanks for sharing your hands on experiences.

"E220_Remote_Switch_FTP" is a standalone FTP sketch for accessing the Battery Monitor "log.txt".  
