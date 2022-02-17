#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266mDNS.h>
#include <Wire.h>
#include <Ticker.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <AddrList.h>
#include "SparkFunBME280.h"

// Comment out the following line to build without HTTP autoupdate.
#define WITH_AUTOUPDATE
#ifdef WITH_AUTOUPDATE
#include <../../autoupdate.h>
// Configure the ticker timer.
Ticker updateTicker;
bool run_update = false;

void UpdateTimer() {
	run_update = true;
}
#endif

//USER CONFIGURED SECTION START//
const char* mqtt_server = "MQTTIP";
const int mqtt_port = 1883;
const char *mqtt_user = NULL;
const char *mqtt_pass = NULL;

String status_log;
String update_status;
float current_temp = 0;
float current_humidity = 0;
float current_pressure = 0;
long lastReconnectAttempt = 0;

// Set up the BME Data
BME280 BMESensor;
Ticker Ticker_SensorRefresh;
bool refresh_sensor = true;

WiFiEventHandler mConnectHandler, mDisConnectHandler, mGotIpHandler;
WiFiClient espClient;
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
PubSubClient client(espClient);

const uint8_t pins[] = { D5, D6, D7, D8 };
bool pin_status[] = { 0, 0, 0, 0 };

void onConnected(const WiFiEventStationModeConnected& event){
	status_log += String(millis(), DEC) + ": Connected to AP.\n";
}

void onDisconnect(const WiFiEventStationModeDisconnected& event){
	status_log += String(millis(), DEC) + ": Station disconnected\n";
	client.disconnect();
}

void onGotIP(const WiFiEventStationModeGotIP& event){
	status_log += String(millis(), DEC) + ": Station connected, IP: " + WiFi.localIP().toString() + "\n";
	#ifdef WITH_AUTOUPDATE
	// Check for update in 10 seconds time.
	updateTicker.attach(10, UpdateTimer);
	#endif
}

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
	Serial.println("Entered config mode");
	Serial.println(WiFi.softAPIP());
	//if you used auto generated SSID, print it
	Serial.println(myWiFiManager->getConfigPortalSSID());
}

void setup() {
	Serial.begin(115200);
	for ( int i = 0; i < 4; i++ ) {
		pinMode(pins[i], OUTPUT);
		digitalWrite(pins[i], LOW);
	}
	pinMode(LED_BUILTIN, OUTPUT);

	mConnectHandler = WiFi.onStationModeConnected(onConnected);
	mDisConnectHandler = WiFi.onStationModeDisconnected(onDisconnect);
	mGotIpHandler = WiFi.onStationModeGotIP(onGotIP);

	// Set power saving for device. WIFI_LIGHT_SLEEP is buggy, so use WIFI_MODEM_SLEEP
	WiFi.persistent(false);
	WiFi.setSleepMode(WIFI_MODEM_SLEEP);
	WiFi.setOutputPower(20);	// 10dBm == 10mW, 14dBm = 25mW, 17dBm = 50mW, 20dBm = 100mW

	// Start connecting to wifi.
	WiFiManager WiFiManager;
	if (!WiFiManager.autoConnect()) {
		Serial.println("failed to connect and hit timeout");
		ESP.reset();
		delay(1000);
	}

	Wire.begin(D2,D1); // initialize I2C that connects to sensor
	Wire.setClock(400000);
	BMESensor.setI2CAddress(0x76);
	status_log += millis();
	if(BMESensor.beginI2C() == false) {
		status_log += F(": Failed to set up BMESensor\n");
	} else {
		status_log += F(": BME connected successfully\n");
		BMESensor.setFilter(2); //0 to 4 is valid. Filter coefficient. See 3.4.4
		BMESensor.setStandbyTime(7); //0 to 7 valid. Time between readings. See table 27.
		BMESensor.setTempOverSample(3); //0 to 16 are valid. 0 disables temp sensing. See table 24.
		BMESensor.setPressureOverSample(3); //0 to 16 are valid. 0 disables pressure sensing. See table 23.
		BMESensor.setHumidityOverSample(3); //0 to 16 are valid. 0 disables humidity sensing. See table 19.
		BMESensor.setMode(MODE_NORMAL); //MODE_SLEEP, MODE_FORCED, MODE_NORMAL is valid. See 3.3
		Ticker_SensorRefresh.attach(20, UpdateSensors);
	}

	httpServer.on("/", handle_root);
	httpServer.on("/toggle", handle_toggle);
	httpServer.on("/reboot", reboot);
	httpUpdater.setup(&httpServer);
	httpServer.begin();

	client.setServer(mqtt_server, mqtt_port);
	client.setCallback(callback);

	MDNS.begin("OutsideMonitor");
	MDNS.addService("http", "tcp", 80);
}

void loop() {
	httpServer.handleClient();
	MDNS.update();

	if ( ! client.loop() && WiFi.status() == WL_CONNECTED ) {
		long now = millis();
		if (now - lastReconnectAttempt > 5000) {
			lastReconnectAttempt = now;
			status_log += String(millis(),DEC) + ": Attempting to connect to MQTT Server\n";
			if (client.connect(WiFi.macAddress().c_str(), mqtt_user, mqtt_pass, "climate/status", 0, 0, "MQTT Disconnected")) {
				status_log += String(millis(),DEC) + ": Connected to MQTT Server\n";

				// Subscribe to topics...
				for ( int i = 1; i < 5; i++ ) {
					String topic = WiFi.macAddress();
					topic += "/output";
					topic += i;
					Serial.print("Subscribing to: ");
					Serial.println(topic);
					status_log += String(millis(), DEC) + ": Subscribing to: " + topic + "\n";
					client.subscribe((char*)topic.c_str());
				}
			}
		}
	}

	#ifdef WITH_AUTOUPDATE
	if ( run_update ) {
		update_status = checkForUpdate();
		updateTicker.attach(8 * 60 * 60, UpdateTimer);
		run_update = false;
	}
	#endif

	if ( refresh_sensor ) {
		Serial.println("Sensor check triggered...");
		digitalWrite(LED_BUILTIN, LOW);

		float newTemp = BMESensor.readTempC();
		if ( newTemp != current_temp ) {
			char temperature_send[5];
			dtostrf(newTemp, 4, 1, temperature_send);
			client.publish("climate/outside/current_temp", temperature_send, true);
			current_temp = newTemp;
		}
		float newHumidity = BMESensor.readFloatHumidity();
		if ( newHumidity != current_humidity ) {
			char humidity_send[5];
			dtostrf(newHumidity, 4, 1, humidity_send);
			client.publish("climate/outside/current_humidity", humidity_send, true);
			current_humidity = newHumidity;
		}
		float newPressure = BMESensor.readFloatPressure() / 100.0F;
		if ( newPressure != current_pressure ) {
			char pressure_send[5];
			dtostrf(newPressure, 4, 1, pressure_send);
			client.publish("climate/outside/current_pressure", pressure_send, true);
			current_pressure = newPressure;
		}
		Serial.print("Temperature: ");
		Serial.print(newTemp);
		Serial.println(" C");
		Serial.print("Humidity: ");
		Serial.print(newHumidity);
		Serial.println(" %");
		Serial.print("Pressure: ");
		Serial.print(newPressure);
		Serial.println(" hPa");
		refresh_sensor = false;
		digitalWrite(LED_BUILTIN, HIGH);
	}

	delay(75);
}

void callback(char* topic, byte* payload, unsigned int length) {
	payload[length] = '\0';
	String Payload = String((char *)payload);
	Payload.toLowerCase();
	Serial.print("[MQTT RX] Topic: "); Serial.println(topic);
	Serial.print("[MQTT RX] Payload: "); Serial.println(Payload);
	int len = strlen(topic);
	String Topic = &topic[len - 7];

	int pin = 255;
	if ( Topic == "output1" ) { pin = 0; }
	if ( Topic == "output2" ) { pin = 1; }
	if ( Topic == "output3" ) { pin = 2; }
	if ( Topic == "output4" ) { pin = 3; }

	if ( pin == 255 ) { return; }

	if ( Payload == "on" ) {
		SetGPIO(pin, HIGH);
	}
	if ( Payload == "off" ) {
		SetGPIO(pin, LOW);
	}
}

void reboot() {
	Serial.println("Rebooting via Web Command...");
	String webpage = "<html><head><meta http-equiv=\"refresh\" content=\"10;url=/\"></head><body>Rebooting</body></html>";
	httpServer.send(200, "text/html", webpage);
	httpServer.handleClient();
	client.disconnect();
	client.loop();
	delay(100);
	ESP.restart();
}

char *uptime(unsigned long milli) {
	static char _return[32];
	unsigned long secs=milli/1000, mins=secs/60;
	unsigned int hours=mins/60, days=hours/24;
	milli-=secs*1000;
	secs-=mins*60;
	mins-=hours*60;
	hours-=days*24;
	sprintf(_return,"%d days %2.2d:%2.2d:%2.2d.%3.3d", (byte)days, (byte)hours, (byte)mins, (byte)secs, (int)milli);
	return _return;
}

void handle_root() {
	String ip_addr = "";
	for (auto a : addrList) {
		ip_addr += a.toString().c_str();
		ip_addr += "<br>";
	}

	String webpage = String("<!DOCTYPE HTML>") +
		F("<head><title>Outside Monitor Status</title></head>") +
		F("<body><table border='1' rules='rows' cellpadding='5'>") +
		F("<tr><td>IP Addresses</td><td>") + ip_addr + F("</td></tr>\n") +
		F("<tr><td>MAC Address</td><td>") + WiFi.macAddress().c_str() + F("</td></tr>\n") +
		F("<tr><td>CPU Speed:</td><td>") + ESP.getCpuFreqMHz() + F(" MHz</td></tr>\n") +
		F("<tr><td>Flash Speed:</td><td>") + (ESP.getFlashChipSpeed() / 1000000) + F(" Mhz</td></tr>\n") +
		F("<tr><td>Flash Real Size:</td><td>") + ESP.getFlashChipRealSize() + F(" bytes</td></tr>\n") +
		F("<tr><td>Flash Mode:</td><td>") +
			(
			ESP.getFlashChipMode() == FM_QIO ? "QIO" :
			ESP.getFlashChipMode() == FM_QOUT ? "QOUT" :
			ESP.getFlashChipMode() == FM_DIO ? "DIO" :
			ESP.getFlashChipMode() == FM_DOUT ? "DOUT" :
			"UNKNOWN") + F("</td></tr>\n") +
		F("<tr><td>Uptime</td><td>") + uptime(millis()) + F("</td></tr>\n") +
		F("<tr><td>Last Reset by</td><td>") + ESP.getResetReason() + F("</td></tr>\n") +
		F("<tr><td>MQTT Server</td><td>") + mqtt_server + F("</td></tr>\n") +
		F("<tr><td>MQTT Status</td><td>") + ( client.connected() ? "Connected" : "Disconnected" ) + F("</td></tr>\n") +
		F("<tr><td>Current Humidity</td><td>") + current_humidity + F(" %</td></tr>\n") +
		F("<tr><td>Current Temp</td><td>") + current_temp + F(" &#8451;</td></tr>\n") +
		F("<tr><td>Current Pressure</td><td>") + current_pressure + F(" hPa</td></tr>\n") +
		F("<tr><td>Output 1</td><td>") + ( pin_status[0] ? "On" : "Off" ) + F(" <a href='/toggle?pin=0'>X</a></td></tr>\n") +
		F("<tr><td>Output 2</td><td>") + ( pin_status[1] ? "On" : "Off" ) + F(" <a href='/toggle?pin=1'>X</a></td></tr>\n") +
		F("<tr><td>Output 3</td><td>") + ( pin_status[2] ? "On" : "Off" ) + F(" <a href='/toggle?pin=2'>X</a></td></tr>\n") +
		F("<tr><td>Output 4</td><td>") + ( pin_status[3] ? "On" : "Off" ) + F(" <a href='/toggle?pin=3'>X</a></td></tr>\n") +
		F("</table>\n") +
		F("<br><b>Status Log:</b><br><pre>") + status_log + F("</pre><br>") +
		F("<b>Update Log:</b><br><pre>") + update_status + F("</pre><br>") +
		F("- <a href=\"/reboot\">Reboot</a><br>- <a href=\"/update\">Update</a><br><br><font size='-1'>") + ESP.getFullVersion() +
		F("</font></body></html>");
	httpServer.sendHeader("Refresh", "10");
	httpServer.send (200, "text/html", webpage );
}

void handle_toggle() {
	uint8_t pin = httpServer.arg("pin").toInt();
	Serial.print("HTTP request to toggle pin: ");
	Serial.println(pins[pin]);
	if ( analogRead(pins[pin]) == 0 ) {
		SetGPIO(pins[pin], HIGH);
	} else {
		SetGPIO(pins[pin], LOW);
	}
	httpServer.sendHeader("Location", String("/"), true);
	httpServer.send( 302, "text/plain", "" );
}

void UpdateSensors() {
	refresh_sensor = true;
}

void SetGPIO(uint8_t gpio, bool value) {
	int max_value = 700;
	if ( value == 1 && pin_status[gpio] != 1 ) {
		pin_status[gpio] = 1;
		for ( int i = 0; i < max_value; i++ ) {
			analogWrite(pins[gpio], i);
			delay(1);
		}
	}
	if ( value == 0 && pin_status[gpio] != 0 ) {
		pin_status[gpio] = 0;
		for ( int i = max_value; i > 0; i-- ) {
			analogWrite(pins[gpio], i);
			delay(1);
		}
	}
}
