E220-Remote-Switch project utilizes two, Ebye E220-900T30D RF modules and two, ESP32 microcontrollers.
Current project status:  INA226 and KY002S have not been implemented in this update.

[E220-Remote-Switch Demo Video](https://drive.google.com/file/d/17vEiuXHJrnUdlfjQXX05xNBP8ygv8aMq/view?usp=sharing)

Demonstation mode:

1.  ESP32 Receiver; push receiver reset button, this puts the ESP32 into deep sleep.
2.  ESP32 Sender; push sender reset button, this sends Wake on radio (WOR) message to wake ESP32 receiver
    from deep sleep.
3.  Open browser to "http://10.0.0.27/relay"; this will create a web request for turning on battery power.
    and start a countdown timer to turn off battery power, then put the ESP32 receiver into deep sleep.
4.  Next web request has a yet to be resolved issue; instead of turning on battery power, this web request ESP32
    Sender sends a WOR message awaking the deep sleeping ESP32 receiver.
5.  Second web request is required to turn on battery power and start the countdown timer.

Three advantages of using the Ebyte, E220-900T30D is increased distance 10 km (estimated at power of 30 dbm) and
Sleep current of 5 uA.  E220-900T30D third feature is the ability to send a WOR message to wake up the receiving 
transciver allowing second message; to turn on battery power. 

Transmit current of 620 mA is almost instantaneous at 30dbm to send; up to 200 bytes, before dropping to sleep 
current.  Receiving current; for a message, 17.2 mA.  Current values are from "Ebytes E220-900T30D User 
Manual". 
