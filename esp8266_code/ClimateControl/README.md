### Parts
[ESP8266 D1 Mini (Wemos)](http://s.click.aliexpress.com/e/cWijEWc0)

### Instructions

This code is to be used with HomeAssistant and its [Generic Thermostat](https://www.home-assistant.io/components/climate.generic_thermostat/) card.

The MQTT topics monitored are:
- climate/heater
- climate/ac/mode/set
- climate/ac/temperature/set
- climate/ac/fan/set

The BME280 sensor readings are published to:
- climate/current_temp
- climate/current_humidity
- climate/current_pressure

HA Configuration example:
```
climate:
  - platform: generic_thermostat
    name: Heater
    heater: switch.heater_control
    target_sensor: sensor.climate_temp
    min_temp: 15
    max_temp: 30
    ac_mode: false
    target_temp: 21
    cold_tolerance: 0.5
    hot_tolerance: 1
    min_cycle_duration:
      seconds: 90
    away_temp: 15
    initial_hvac_mode: heat

  - platform: mqtt
    name: Air Conditioner
    modes:
      - "off"
      - "heat"
      - "cool"
      - "dry"
      - "auto"
    fan_modes:
      - "auto"
      - "high"
      - "medium"
      - "low"
    min_temp: 17
    max_temp: 30
    current_temperature_topic: "climate/current_temp"
    mode_command_topic: "climate/ac/mode/set"
    temperature_command_topic: "climate/ac/temperature/set"
    fan_mode_command_topic: "climate/ac/fan/set"
    retain: true

switch:
  - platform: mqtt
    name: "Heater Control"
    state_topic: "climate/heater"
    command_topic: "climate/heater"
    payload_on: "on"
    payload_off: "off"
    icon: mdi:power

sensor:
  - platform: mqtt
    name: "Climate Temp"
    state_topic: "climate/current_temp"

  - platform: mqtt
    name: "Climate Humidity"
    state_topic: "climate/current_humidity"

  - platform: mqtt
    name: "Climate Pressure"
    state_topic: "climate/current_pressure"
```
