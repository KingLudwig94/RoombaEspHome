This project has a DHT22 temp/humidity sensor, and a 4 channel MOSFET switch.

The DHT feeds via MQTT topics to Home Assistant, while the outputs for the MOSFETs control various things like lights etc.

### Parts

[4 Channel MOSFET board](https://www.aliexpress.com/item/Four-Channel-4-Route-MOSFET-Button-IRF540-V4-0-MOSFET-Switch-Module-For-Arduino/32816652409.html)

[DHT22 Sensor](https://www.aliexpress.com/item/Free-shipping-DHT22-AM2302-replace-SHT11-SHT15-Humidity-temperature-and-humidity-sensor/1872664976.html)

[ESP8266 D1 Mini (Wemos)](https://www.aliexpress.com/item/ESP8266-ESP12-ESP-12-WeMos-D1-Mini-Module-Wemos-D1-Mini-WiFi-Development-Board-Micro-USB/32869375284.html)

### Software

[Platform IO](https://platformio.org/)

### Instructions

- Hook the DHT22 to pin D3
- Hook the four channels of the MOSFET to D5, D6, D7, D8
- Build & install

Trigger by MQTT packet

