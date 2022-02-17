// Auto generated code by esphome
// ========== AUTO GENERATED INCLUDE BLOCK BEGIN ===========
#include "esphome.h"
using namespace esphome;
using std::isnan;
using std::min;
using std::max;
using namespace text_sensor;
using namespace switch_;
logger::Logger *logger_logger;
web_server_base::WebServerBase *web_server_base_webserverbase;
captive_portal::CaptivePortal *captive_portal_captiveportal;
wifi::WiFiComponent *wifi_wificomponent;
mdns::MDNSComponent *mdns_mdnscomponent;
ota::OTAComponent *ota_otacomponent;
api::APIServer *api_apiserver;
using namespace api;
using namespace sensor;
preferences::IntervalSyncer *preferences_intervalsyncer;
sensor::Sensor *sensor_sensor;
sensor::Sensor *sensor_sensor_2;
sensor::Sensor *sensor_sensor_3;
sensor::Sensor *sensor_sensor_4;
sensor::Sensor *sensor_sensor_5;
sensor::Sensor *sensor_sensor_6;
text_sensor::TextSensor *text_sensor_textsensor;
gpio::GPIOSwitch *roomba_button;
switch_::SwitchTurnOnTrigger *switch__switchturnontrigger;
Automation<> *automation;
LambdaAction<> *lambdaaction;
DelayAction<> *delayaction;
switch_::TurnOffAction<> *switch__turnoffaction;
esphome::esp8266::ESP8266GPIOPin *esphome_esp8266_esp8266gpiopin;
#define yield() esphome::yield()
#define millis() esphome::millis()
#define micros() esphome::micros()
#define delay(x) esphome::delay(x)
#define delayMicroseconds(x) esphome::delayMicroseconds(x)
#include "roomba_component.h"
// ========== AUTO GENERATED INCLUDE BLOCK END ==========="

void setup() {
  // ========== AUTO GENERATED CODE BEGIN ===========
  // esp8266:
  //   board: d1_mini
  //   framework:
  //     version: 3.0.2
  //     source: ~3.30002.0
  //     platform_version: platformio/espressif8266 @ 3.2.0
  //   restore_from_flash: false
  //   board_flash_mode: dout
  esphome::esp8266::setup_preferences();
  // async_tcp:
  //   {}
  // esphome:
  //   name: roomba
  //   includes:
  //   - roomba_component.h
  //   libraries:
  //   - Roomba=https:github.com/davidecavestro/Roomba.git
  //   platformio_options:
  //     build_flags: -D HAVE_HWSERIAL0
  //   build_path: .esphome/build/roomba
  //   name_add_mac_suffix: false
  App.pre_setup("roomba", __DATE__ ", " __TIME__, false);
  // text_sensor:
  // switch:
  // logger:
  //   id: logger_logger
  //   baud_rate: 115200
  //   tx_buffer_size: 512
  //   deassert_rts_dtr: false
  //   hardware_uart: UART0
  //   level: DEBUG
  //   logs: {}
  //   esp8266_store_log_strings_in_flash: true
  logger_logger = new logger::Logger(115200, 512, logger::UART_SELECTION_UART0);
  logger_logger->pre_setup();
  logger_logger->set_component_source("logger");
  App.register_component(logger_logger);
  // web_server_base:
  //   id: web_server_base_webserverbase
  web_server_base_webserverbase = new web_server_base::WebServerBase();
  web_server_base_webserverbase->set_component_source("web_server_base");
  App.register_component(web_server_base_webserverbase);
  // captive_portal:
  //   id: captive_portal_captiveportal
  //   web_server_base_id: web_server_base_webserverbase
  captive_portal_captiveportal = new captive_portal::CaptivePortal(web_server_base_webserverbase);
  captive_portal_captiveportal->set_component_source("captive_portal");
  App.register_component(captive_portal_captiveportal);
  // wifi:
  //   ap:
  //     ssid: Roomba Fallback Hotspot
  //     password: asdasdafetgsfds45asd687aw
  //     id: wifi_wifiap
  //     ap_timeout: 1min
  //   id: wifi_wificomponent
  //   domain: .local
  //   reboot_timeout: 15min
  //   power_save_mode: NONE
  //   fast_connect: false
  //   output_power: 20.0
  //   networks:
  //   - ssid: iBargaggez
  //     password: '#Ramaiolo1994'
  //     id: wifi_wifiap_2
  //     priority: 0.0
  //   use_address: roomba.local
  wifi_wificomponent = new wifi::WiFiComponent();
  wifi_wificomponent->set_use_address("roomba.local");
  wifi::WiFiAP wifi_wifiap_2 = wifi::WiFiAP();
  wifi_wifiap_2.set_ssid("iBargaggez");
  wifi_wifiap_2.set_password("#Ramaiolo1994");
  wifi_wifiap_2.set_priority(0.0f);
  wifi_wificomponent->add_sta(wifi_wifiap_2);
  wifi::WiFiAP wifi_wifiap = wifi::WiFiAP();
  wifi_wifiap.set_ssid("Roomba Fallback Hotspot");
  wifi_wifiap.set_password("asdasdafetgsfds45asd687aw");
  wifi_wificomponent->set_ap(wifi_wifiap);
  wifi_wificomponent->set_ap_timeout(60000);
  wifi_wificomponent->set_reboot_timeout(900000);
  wifi_wificomponent->set_power_save_mode(wifi::WIFI_POWER_SAVE_NONE);
  wifi_wificomponent->set_fast_connect(false);
  wifi_wificomponent->set_output_power(20.0f);
  wifi_wificomponent->set_component_source("wifi");
  App.register_component(wifi_wificomponent);
  // mdns:
  //   id: mdns_mdnscomponent
  //   disabled: false
  mdns_mdnscomponent = new mdns::MDNSComponent();
  mdns_mdnscomponent->set_component_source("mdns");
  App.register_component(mdns_mdnscomponent);
  // ota:
  //   id: ota_otacomponent
  //   safe_mode: true
  //   port: 8266
  //   reboot_timeout: 5min
  //   num_attempts: 10
  ota_otacomponent = new ota::OTAComponent();
  ota_otacomponent->set_port(8266);
  ota_otacomponent->set_component_source("ota");
  App.register_component(ota_otacomponent);
  if (ota_otacomponent->should_enter_safe_mode(10, 300000)) return;
  // api:
  //   id: api_apiserver
  //   port: 6053
  //   password: ''
  //   reboot_timeout: 15min
  api_apiserver = new api::APIServer();
  api_apiserver->set_component_source("api");
  App.register_component(api_apiserver);
  api_apiserver->set_port(6053);
  api_apiserver->set_password("");
  api_apiserver->set_reboot_timeout(900000);
  // sensor:
  // substitutions:
  //   name: roomba
  //   init: RoombaComponent::instance(2, 5, 4, Roomba::Baud115200, 30000);
  // preferences:
  //   id: preferences_intervalsyncer
  //   flash_write_interval: 60s
  preferences_intervalsyncer = new preferences::IntervalSyncer();
  preferences_intervalsyncer->set_write_interval(60000);
  preferences_intervalsyncer->set_component_source("preferences");
  App.register_component(preferences_intervalsyncer);
  // custom_component:
  //   lambda: !lambda |-
  //     auto r = RoombaComponent::instance(2, 5, 4, Roomba::Baud115200, 30000);
  //     return {r};
  //   id: custom_component_customcomponentconstructor
  custom_component::CustomComponentConstructor custom_component_customcomponentconstructor = custom_component::CustomComponentConstructor([=]() -> std::vector<Component *> {
      #line 47 "roomba.yaml"
      auto r = RoombaComponent::instance(2, 5, 4, Roomba::Baud115200, 30000);
      return {r};
  });
  // sensor.custom:
  //   platform: custom
  //   lambda: !lambda |-
  //     auto r = RoombaComponent::instance(2, 5, 4, Roomba::Baud115200, 30000);
  //     return {r->distanceSensor, r->voltageSensor, r->currentSensor, r->chargeSensor, r->capacitySensor, r->batteryPercentSensor};
  //   sensors:
  //   - name: roomba distance
  //     unit_of_measurement: mm
  //     accuracy_decimals: 0
  //     disabled_by_default: false
  //     id: sensor_sensor
  //     force_update: false
  //   - name: roomba voltage
  //     unit_of_measurement: mV
  //     accuracy_decimals: 0
  //     disabled_by_default: false
  //     id: sensor_sensor_2
  //     force_update: false
  //   - name: roomba current
  //     unit_of_measurement: mA
  //     accuracy_decimals: 0
  //     disabled_by_default: false
  //     id: sensor_sensor_3
  //     force_update: false
  //   - name: roomba charge
  //     unit_of_measurement: mAh
  //     accuracy_decimals: 0
  //     disabled_by_default: false
  //     id: sensor_sensor_4
  //     force_update: false
  //   - name: roomba capacity
  //     unit_of_measurement: mAh
  //     accuracy_decimals: 0
  //     disabled_by_default: false
  //     id: sensor_sensor_5
  //     force_update: false
  //   - name: roomba battery
  //     unit_of_measurement: '%'
  //     accuracy_decimals: 0
  //     disabled_by_default: false
  //     id: sensor_sensor_6
  //     force_update: false
  //   id: custom_customsensorconstructor
  custom::CustomSensorConstructor custom_customsensorconstructor = custom::CustomSensorConstructor([=]() -> std::vector<sensor::Sensor *> {
      #line 52 "roomba.yaml"
      auto r = RoombaComponent::instance(2, 5, 4, Roomba::Baud115200, 30000);
      return {r->distanceSensor, r->voltageSensor, r->currentSensor, r->chargeSensor, r->capacitySensor, r->batteryPercentSensor};
  });
  sensor_sensor = custom_customsensorconstructor.get_sensor(0);
  App.register_sensor(sensor_sensor);
  sensor_sensor->set_name("roomba distance");
  sensor_sensor->set_disabled_by_default(false);
  sensor_sensor->set_unit_of_measurement("mm");
  sensor_sensor->set_accuracy_decimals(0);
  sensor_sensor->set_force_update(false);
  sensor_sensor_2 = custom_customsensorconstructor.get_sensor(1);
  App.register_sensor(sensor_sensor_2);
  sensor_sensor_2->set_name("roomba voltage");
  sensor_sensor_2->set_disabled_by_default(false);
  sensor_sensor_2->set_unit_of_measurement("mV");
  sensor_sensor_2->set_accuracy_decimals(0);
  sensor_sensor_2->set_force_update(false);
  sensor_sensor_3 = custom_customsensorconstructor.get_sensor(2);
  App.register_sensor(sensor_sensor_3);
  sensor_sensor_3->set_name("roomba current");
  sensor_sensor_3->set_disabled_by_default(false);
  sensor_sensor_3->set_unit_of_measurement("mA");
  sensor_sensor_3->set_accuracy_decimals(0);
  sensor_sensor_3->set_force_update(false);
  sensor_sensor_4 = custom_customsensorconstructor.get_sensor(3);
  App.register_sensor(sensor_sensor_4);
  sensor_sensor_4->set_name("roomba charge");
  sensor_sensor_4->set_disabled_by_default(false);
  sensor_sensor_4->set_unit_of_measurement("mAh");
  sensor_sensor_4->set_accuracy_decimals(0);
  sensor_sensor_4->set_force_update(false);
  sensor_sensor_5 = custom_customsensorconstructor.get_sensor(4);
  App.register_sensor(sensor_sensor_5);
  sensor_sensor_5->set_name("roomba capacity");
  sensor_sensor_5->set_disabled_by_default(false);
  sensor_sensor_5->set_unit_of_measurement("mAh");
  sensor_sensor_5->set_accuracy_decimals(0);
  sensor_sensor_5->set_force_update(false);
  sensor_sensor_6 = custom_customsensorconstructor.get_sensor(5);
  App.register_sensor(sensor_sensor_6);
  sensor_sensor_6->set_name("roomba battery");
  sensor_sensor_6->set_disabled_by_default(false);
  sensor_sensor_6->set_unit_of_measurement("%");
  sensor_sensor_6->set_accuracy_decimals(0);
  sensor_sensor_6->set_force_update(false);
  // text_sensor.custom:
  //   platform: custom
  //   lambda: !lambda |-
  //     auto r = RoombaComponent::instance(2, 5, 4, Roomba::Baud115200, 30000);
  //     return {r->activitySensor};
  //   text_sensors:
  //   - name: roomba activity
  //     disabled_by_default: false
  //     id: text_sensor_textsensor
  //   id: custom_customtextsensorconstructor
  custom::CustomTextSensorConstructor custom_customtextsensorconstructor = custom::CustomTextSensorConstructor([=]() -> std::vector<text_sensor::TextSensor *> {
      #line 77 "roomba.yaml"
      auto r = RoombaComponent::instance(2, 5, 4, Roomba::Baud115200, 30000);
      return {r->activitySensor};
  });
  text_sensor_textsensor = custom_customtextsensorconstructor.get_text_sensor(0);
  App.register_text_sensor(text_sensor_textsensor);
  text_sensor_textsensor->set_name("roomba activity");
  text_sensor_textsensor->set_disabled_by_default(false);
  // switch.gpio:
  //   platform: gpio
  //   id: roomba_button
  //   name: Roomba virtual button
  //   pin:
  //     number: 0
  //     mode:
  //       output: true
  //       analog: false
  //       input: false
  //       open_drain: false
  //       pullup: false
  //       pulldown: false
  //     id: esphome_esp8266_esp8266gpiopin
  //     inverted: false
  //   on_turn_on:
  //   - then:
  //     - logger.log:
  //         format: Roomba virtual button pressed!
  //         args: []
  //         level: DEBUG
  //         tag: main
  //       type_id: lambdaaction
  //     - delay: !lambda |-
  //         return 400;
  //       type_id: delayaction
  //     - switch.turn_off:
  //         id: roomba_button
  //       type_id: switch__turnoffaction
  //     automation_id: automation
  //     trigger_id: switch__switchturnontrigger
  //   disabled_by_default: false
  //   restore_mode: RESTORE_DEFAULT_OFF
  //   interlock_wait_time: 0ms
  roomba_button = new gpio::GPIOSwitch();
  roomba_button->set_component_source("gpio.switch");
  App.register_component(roomba_button);
  App.register_switch(roomba_button);
  roomba_button->set_name("Roomba virtual button");
  roomba_button->set_disabled_by_default(false);
  switch__switchturnontrigger = new switch_::SwitchTurnOnTrigger(roomba_button);
  automation = new Automation<>(switch__switchturnontrigger);
  lambdaaction = new LambdaAction<>([=]() -> void {
      ESP_LOGD("main", "Roomba virtual button pressed!");
  });
  delayaction = new DelayAction<>();
  delayaction->set_component_source("switch");
  App.register_component(delayaction);
  delayaction->set_delay([=]() -> uint32_t {
      #line 91 "roomba.yaml"
      return 400;
  });
  switch__turnoffaction = new switch_::TurnOffAction<>(roomba_button);
  automation->add_actions({lambdaaction, delayaction, switch__turnoffaction});
  esphome_esp8266_esp8266gpiopin = new esphome::esp8266::ESP8266GPIOPin();
  esphome_esp8266_esp8266gpiopin->set_pin(0);
  esphome_esp8266_esp8266gpiopin->set_inverted(false);
  esphome_esp8266_esp8266gpiopin->set_flags(gpio::Flags::FLAG_OUTPUT);
  roomba_button->set_pin(esphome_esp8266_esp8266gpiopin);
  roomba_button->set_restore_mode(gpio::GPIO_SWITCH_RESTORE_DEFAULT_OFF);
  // socket:
  //   implementation: lwip_tcp
  // network:
  //   {}
  // =========== AUTO GENERATED CODE END ============
  App.setup();
}

void loop() {
  App.loop();
}
