#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266HTTPUpdateServer.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Coolix.h>
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
const char* mqtt_server = "ha.crc.id.au";
const int mqtt_port = 1883;
const char *mqtt_user = NULL;
const char *mqtt_pass = NULL;

// Define Pins
#define HeaterPin D6
#define HeatOn HIGH
#define HeatOff LOW
#define IR_PIN D7

// Define the world.
WiFiEventHandler mConnectHandler, mDisConnectHandler, mGotIpHandler;
WiFiClient espClient;
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
PubSubClient client(espClient);

// Set up the BME Data
BME280 BMESensor;
Ticker Ticker_SensorRefresh;
bool refresh_sensor = true;

// Set up the A/C unit
IRCoolixAC ac(IR_PIN);

// Current observations for the climate readings.
float current_temp;
float current_humidity;
float current_pressure;
long lastReconnectAttempt = 0;
String update_status;

void onConnected(const WiFiEventStationModeConnected& event){
	update_status += millis();
	update_status += ": Connected to AP.";
	update_status += "\n";
}

void onDisconnect(const WiFiEventStationModeDisconnected& event){
	update_status += millis();
	update_status += ": Station disconnected";
	update_status += "\n";
	client.disconnect();
}

void onGotIP(const WiFiEventStationModeGotIP& event){
	update_status += millis();
	update_status += ": Got IP: ";
	for (auto a : addrList) {
		update_status += a.toString().c_str();
		update_status += " ";
	}
	update_status += "\n";
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

	// Set up the pin for the heater.
	pinMode(HeaterPin, OUTPUT);
	digitalWrite(HeaterPin, HeatOff);

	// Set up wifi event handlers.
	mConnectHandler = WiFi.onStationModeConnected(onConnected);
	mDisConnectHandler = WiFi.onStationModeDisconnected(onDisconnect);
	mGotIpHandler = WiFi.onStationModeGotIP(onGotIP);

	// Set power saving for device. WIFI_LIGHT_SLEEP is buggy, so use WIFI_MODEM_SLEEP
	WiFi.setSleepMode(WIFI_MODEM_SLEEP);
	WiFi.setOutputPower(18);	// 10dBm == 10mW, 14dBm = 25mW, 17dBm = 50mW, 20dBm = 100mW

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
	update_status += millis();
	if(BMESensor.beginI2C() == false) {
		update_status += F(": Failed to set up BMESensor\n");
	} else {
		update_status += F(": BME connected successfully\n");
		BMESensor.setFilter(2); //0 to 4 is valid. Filter coefficient. See 3.4.4
		BMESensor.setStandbyTime(7); //0 to 7 valid. Time between readings. See table 27.
		BMESensor.setTempOverSample(3); //0 to 16 are valid. 0 disables temp sensing. See table 24.
		BMESensor.setPressureOverSample(3); //0 to 16 are valid. 0 disables pressure sensing. See table 23.
		BMESensor.setHumidityOverSample(3); //0 to 16 are valid. 0 disables humidity sensing. See table 19.
		BMESensor.setMode(MODE_NORMAL); //MODE_SLEEP, MODE_FORCED, MODE_NORMAL is valid. See 3.3
		Ticker_SensorRefresh.attach(20, UpdateSensors);
	}

	// Set up the IR LED pin
	pinMode(IR_PIN, OUTPUT);
	ac.begin();
	ac.setPower(false);
	ac.send();

	// Set up the build in LED for heater status indication.
	// On the D1 Mini, this has a pullup, so LOW turns the LED on.
	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, HIGH);

	httpServer.on("/", handle_root);
	httpServer.on("/reboot", reboot);
	httpUpdater.setup(&httpServer);
	httpServer.begin();

	#ifdef WITH_AUTOUPDATE
	Serial.println("Autoupdate enabled at compile time...");
	#endif

	client.setServer(mqtt_server, mqtt_port);
	client.setCallback(callback);
}

void loop() {
	httpServer.handleClient();

	if ( ! client.loop() && WiFi.status() == WL_CONNECTED ) {
		// Always turn off the heater if we don't have wifi.
		// Otherwise, we won't get the OFF signal.
		digitalWrite(LED_BUILTIN, HIGH);
		digitalWrite(HeaterPin, HeatOff);

		long now = millis();
		if (now - lastReconnectAttempt > 5000) {
			lastReconnectAttempt = now;
			update_status += millis();
			update_status += ": Attempting to connect to MQTT Server\n";
			if (client.connect(WiFi.macAddress().c_str(), mqtt_user, mqtt_pass, "climate/status", 0, 0, "MQTT Disconnected")) {
				update_status += millis();
				update_status += ": Connected to MQTT Server\n";
				client.subscribe("climate/heater");
				client.subscribe("climate/ac/mode/set");
				client.subscribe("climate/ac/temperature/set");
				client.subscribe("climate/ac/fan/set");
			}
		}
	}

	#ifdef WITH_AUTOUPDATE
	if ( run_update ) {
		Serial.println(checkForUpdate());
		updateTicker.attach(8 * 60 * 60, UpdateTimer);
		run_update = false;
		Serial.print(millis());
		Serial.println(": Finished auto-update check...");
	}
	#endif

	if ( refresh_sensor ) {
		Serial.println("Sensor check triggered...");
		digitalWrite(LED_BUILTIN, LOW);

		float newTemp = BMESensor.readTempC();
		if ( newTemp != current_temp ) {
			char temperature_send[5];
			dtostrf(newTemp, 4, 1, temperature_send);
			client.publish("climate/current_temp", temperature_send, true);
			current_temp = newTemp;
		}
		float newHumidity = BMESensor.readFloatHumidity();
		if ( newHumidity != current_humidity ) {
			char humidity_send[5];
			dtostrf(newHumidity, 4, 1, humidity_send);
			client.publish("climate/current_humidity", humidity_send, true);
			current_humidity = newHumidity;
		}
		float newPressure = BMESensor.readFloatPressure() / 100.0F;
		if ( newPressure != current_pressure ) {
			char pressure_send[5];
			dtostrf(newPressure, 4, 1, pressure_send);
			client.publish("climate/current_pressure", pressure_send, true);
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

		// Turn off the LED if the heater isn't on as well...
		if ( ! digitalRead(HeaterPin) ) {
			digitalWrite(LED_BUILTIN, HIGH);
		}
	}
	delay(50);
}

void UpdateSensors() {
	refresh_sensor = true;
}

void callback(char* topic, byte* payload, unsigned int length)
{
	String newTopic = topic;
	payload[length] = '\0';
	String newPayload = String((char *)payload);
	newPayload.toLowerCase();

	if (newTopic == "climate/heater") {
		if ( newPayload == "off" ) {
			digitalWrite(LED_BUILTIN, HIGH);
			digitalWrite(HeaterPin, HeatOff);
		}
		if ( newPayload == "on" ) {
			digitalWrite(LED_BUILTIN, LOW);
			digitalWrite(HeaterPin, HeatOn);
		}
		return;
	}

	// Handle setting a new target temperature
	if (newTopic == "climate/ac/temperature/set") {
		uint8_t new_temp = newPayload.toInt();
		if ( new_temp == ac.getTemp() ) { return; }
		ac.setTemp(new_temp);
		if ( ac.getPower() == false ) {
			return;
		}
	}

	// Handle setting a new mode
	if (newTopic == "climate/ac/mode/set") {
		if ( newPayload == "off" ) {
			if ( ac.getPower() ) {
				ac.setMode(kCoolixOff);
				ac.setPower(false);
				ac.send();
			}
			return;
		}
		if ( newPayload == "auto" ) {
			if ( ac.getMode() == kCoolixAuto ) { return; }
			ac.setMode(kCoolixAuto);
		}
		if ( newPayload == "heat" ) {
			if ( ac.getMode() == kCoolixHeat ) { return; }
			ac.setMode(kCoolixHeat);
		}
		if ( newPayload == "cool" ) {
			if ( ac.getMode() == kCoolixCool ) { return; }
			ac.setMode(kCoolixCool);
		}
		if ( newPayload == "dry" ) {
			if ( ac.getMode() == kCoolixDry ) { return; }
			ac.setMode(kCoolixDry);
		}
		ac.setPower(true);
	}

	// Handle setting a new fan speed
	if ( newTopic == "climate/ac/fan/set") {
		if ( newPayload == "auto" ) { ac.setFan(kCoolixFanAuto); }
		if ( newPayload == "low") { ac.setFan(kCoolixFanMin); }
		if ( newPayload == "medium") { ac.setFan(kCoolixFanMed); }
		if ( newPayload == "high") { ac.setFan(kCoolixFanMax); }
	}

	if ( ac.getPower() ) {
		ac.send();
	}
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
		F("<head><title>Climate Control Status</title></head>") +
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
		F("<tr><td>Current Temp</td><td>") + current_temp + F(" &#8451;</td></tr>\n") +
		F("<tr><td>Current Humidity</td><td>") + current_humidity + F(" %</td></tr>\n") +
		F("<tr><td>Current Pressure</td><td>") + current_pressure +	F(" hPa</td></tr>\n") +
		F("<tr><td>Heater Status</td><td>") + ( digitalRead(HeaterPin) ? "On" : "Off" ) + F("</td></tr>\n") +
		F("</table>") +
		F("<br>A/C Status:<br><pre>") + ac.toString() + F("</pre>\n") +
		F("<br>Update Status:<br><pre>") + update_status +	F("</pre>\n<br>") +
		F("- <a href=\"/reboot\">Reboot</a><br>\n") +
		F("- <a href=\"/update\">Update</a><br>\n") +
		F("<br><font size=\"-1\">") + ESP.getFullVersion() + F("</font></body></html>");
	httpServer.sendHeader("Refresh", "10");
	httpServer.send(200, "text/html", webpage);
}

void reboot() {
	Serial.println(F("Reboot triggered from Web UI"));
	String webpage = F("<html><head><meta http-equiv=\"refresh\" content=\"10;url=/\"></head><body>Rebooting</body></html>");
	httpServer.send(200, "text/html", webpage);
	client.publish("climate/status", "Rebooting");
	httpServer.handleClient();
	client.loop();
	delay(500);
	ESP.restart();
}
