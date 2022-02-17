# RGB Indicator

Equipment:
* ESP8266 D1 Mini
* 13 x WS2812b LED strip @ 60 per meter
* 3D printed files in Enclosure directory

## Sending alerts

The JSON endpoint is `http://$ip/json`.

Post data in the following format:
```
'{"id": 0, "colour": { r: 255, g: 0, b: 255} , repeat:2}'
```
### Current effect list

* id: 0 = Fade to given RGB colour
* id: 1 = Wipe to given RGB colour
* id: 2 = 500ms flash of given RGB colour

### Attributes

* repeat: Number of times to repeat the effect. Default is 1.
