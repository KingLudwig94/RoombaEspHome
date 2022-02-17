### Parts
[ESP8266 D1 Mini (Wemos)](http://s.click.aliexpress.com/e/cWijEWc0)

[Mini DC-DC power supply](http://s.click.aliexpress.com/e/b6Rovlyy)

### Software
[Platform IO](https://platformio.org/)

### Instructions
Use the Mini DC-DC power supply to convert from the unsmoothed output of the DC bridge rectifier on the board and adjusted to output 5v.

I added a diode in the positive line to stop the motor from causing a massive voltage drop when starting up.
I also added two big capacitors on both sides of the DC-DC supply. 1000uF 50V on the high voltage side, 2200uF 16v on the 5v output.
Feed the 5v output into the 5v pin on the D1 mini.

The LEDs give a suitable 3.3v 'logic high' to connect directly to a GPIO pin on the D1 mini.

Close limit LED connects to pin D7, Open limit LED to pin D6.

Still working out how to emulate a button press to operate the door.

### Pictures

![Pre-modification](https://git.crc.id.au/netwiz/ESP8266_Code/raw/branch/master/GarageDoor/GarageDoor-0.jpg)
![Post-modification](https://git.crc.id.au/netwiz/ESP8266_Code/raw/branch/master/GarageDoor/GarageDoor-1.jpg)
