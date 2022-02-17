Rewritten to use the new MQTT Vacuum integration in Home Assistant:
	https://www.home-assistant.io/integrations/vacuum.mqtt/

Configure in Home Assistant `configuration.yaml` as follows:
```
vacuum:
- platform: mqtt
  name: Roomba
  schema: state
  command_topic: "roomba/commands"
  retain: true
  state_topic: "roomba/state"
  supported_features:
    - start
    - stop
    - return_home
    - battery
```
