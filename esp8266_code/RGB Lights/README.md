### Parts
[ESP8266 D1 Mini (Wemos)](http://s.click.aliexpress.com/e/cWijEWc0)

[WS2812 Addressable LEDs](http://s.click.aliexpress.com/e/bIJ5tCJI)

#### About

This project was originally forked from [bruhautomation's](https://github.com/bruhautomation/ESP-MQTT-JSON-Digital-LEDs) code - but with a lot of it re-written for functionality / optimisations.

#### Home Assistant configuration

Within *configuration.yaml*, add the following:

```
light:
  - platform: mqtt
    schema: json
    name: "Bed Strip"
    state_topic: "lights/bedroom"
    command_topic: "lights/bedroom/set"
    effect: true
    effect_list:
      - rainbow
      - solid
    brightness: true
    rgb: true
    optimistic: false
    qos: 0
```

To change the MQTT topic used, adjust the state_topic and command_topic in both the .ino code and the above config.

I have these configured and installed behind the headboard of my bed projecting onto the wall.
