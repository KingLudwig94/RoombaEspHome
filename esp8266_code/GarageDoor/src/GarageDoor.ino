#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266mDNS.h>
#include <Ticker.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <AddrList.h>
#include <PubSubClient.h>

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

// USER CONFIGURED SECTION START //
const char* mqtt_server = "ha.crc.id.au";
const int mqtt_port = 1883;
const char *mqtt_user = NULL;
const char *mqtt_pass = NULL;

#define PIN_UP_LED D6
#define PIN_DOWN_LED D7

#define PIN_RELAY D5
#define RELAY_LATCH LOW
#define RELAY_RELEASE HIGH

#define STATE_UNKNOWN 0
#define STATE_CLOSED 1
#define STATE_OPEN 2
#define STATE_CLOSING 3
#define STATE_OPENING 4
#define STATE_ERROR 5

uint8_t current_state = 0;
long lastReconnectAttempt = 0;
String update_status;
String status;
bool UpLED = false;
bool DownLED = false;
uint8_t downChanges = 0;
uint8_t upChanges = 0;

Ticker Ticker_DetectState;
bool run_DetectState = false;

WiFiEventHandler mConnectHandler, mDisConnectHandler, mGotIpHandler;
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
WiFiClient espClient;
PubSubClient client(espClient);

void onConnected(const WiFiEventStationModeConnected& event){
	update_status += millis();
	update_status += F(": Connected to AP.\n");
	Serial.print(millis()); Serial.println(": onConnected completed.");
}

void onDisconnect(const WiFiEventStationModeDisconnected& event){
	update_status += millis();
	update_status += F(": Station disconnected\n");
	client.disconnect();
	Serial.print(millis()); Serial.println(": onDisconnect completed.");
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

	// Set up pins early
	pinMode(PIN_UP_LED, INPUT);
	pinMode(PIN_DOWN_LED, INPUT);
	pinMode(PIN_RELAY, OUTPUT);
	digitalWrite(PIN_RELAY, RELAY_RELEASE);

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

	httpServer.on("/", handle_root);
	httpServer.on("/reboot", reboot);
	httpUpdater.setup(&httpServer);
	httpServer.begin();

	client.setServer(mqtt_server, mqtt_port);
	client.setCallback(callback);

	Ticker_DetectState.attach(1, RunDetectState);

	MDNS.begin("GarageDoor");
	MDNS.addService("http", "tcp", 80);
}

void loop() {
	if ( ! client.loop() && WiFi.status() == WL_CONNECTED ) {
		current_state = STATE_UNKNOWN;
		long now = millis();
		if (now - lastReconnectAttempt > 5000) {
			lastReconnectAttempt = now;
			update_status += millis();
			update_status += F(": Attempting to connect to MQTT Server\n");
			Serial.println(F("Attempting to connect to MQTT server..."));
			if (client.connect(WiFi.macAddress().c_str(), mqtt_user, mqtt_pass, "garage/status", 0, 0, "Not responding")) {
				update_status += millis();
				update_status += ": Connected to MQTT Server\n";
				client.subscribe("garage/commands");
			}
		}
	}

	// Read each pin 10 times and go with the highest state value.
	uint8_t up_high = 0;
	uint8_t up_low = 0;
	uint8_t down_high = 0;
	uint8_t down_low = 0;
	for ( int i = 0; i <= 10; i++ ) {
		if ( digitalRead(PIN_UP_LED) ) { up_high++; } else { up_low++; }
		if ( digitalRead(PIN_DOWN_LED) ) { down_high++; } else { down_low++; }
		delay(1);
	}

	// Act on what has the highest values.
	if ( up_high > up_low ) { if ( UpLED == LOW ) { upChanges++; } UpLED = HIGH; }
	if ( up_high < up_low ) { if ( UpLED == HIGH ) { upChanges++; } UpLED = LOW; }
	if ( down_high > down_low ) { if ( DownLED == LOW ) { downChanges++; } DownLED = HIGH; }
	if ( down_high < down_low ) { if ( DownLED == HIGH ) { downChanges++; }; DownLED = LOW; }

	#ifdef WITH_AUTOUPDATE
	if ( run_update ) {
		Serial.println(checkForUpdate());
		updateTicker.attach(8 * 60 * 60, UpdateTimer);
		run_update = false;
		Serial.print(millis());
		Serial.println(F(": Finished auto-update check..."));
	}
	#endif

	// Do we need to run DetectState?
	if ( run_DetectState ) {
		DetectState();
		run_DetectState = false;
	}

	httpServer.handleClient();
	MDNS.update();
	delay(40);
}

void RunDetectState() {
	run_DetectState = true;
}

void DetectState() {
	Serial.print("Running at millis: "); Serial.println(millis());
	Serial.print("upChanges = "); Serial.println(upChanges);
	Serial.print("downChanges = "); Serial.println(downChanges);
	Serial.print("PIN_UP_LED = "); Serial.println(UpLED);
	Serial.print("PIN_DOWN_LED = "); Serial.println(DownLED);
	Serial.println("");

	// Up LED is on all the time
	if ( upChanges == 0 && downChanges == 0 && UpLED == true && DownLED == false ) {
		status = "OPEN\n";
		if ( current_state != STATE_OPEN && client.connected() ) {
			current_state = STATE_OPEN;
			client.publish("garage/status", "open", true);
		}
	} else

	// Down LED is on all the time
	if ( upChanges == 0 && downChanges == 0 && UpLED == false && DownLED == true ) {
		status = "CLOSED\n";
		if ( current_state != STATE_CLOSED && client.connected() ) {
			current_state = STATE_CLOSED;
			client.publish("garage/status", "closed", true);
		}
	} else

	// Down LED is flashing, Up is off = Closing
	if ( upChanges == 0 && downChanges > 1 && downChanges < 5 ) {
		status = "CLOSING\n";
		if ( current_state != STATE_CLOSING && client.connected() ) {
			current_state = STATE_CLOSING;
			client.publish("garage/status", "closing", true);
		}
	} else

	// Up LED is flashing, Down is off = Opening
	if ( downChanges == 0 && upChanges > 1 && upChanges < 5 ) {
		status = "OPENING\n";
		if ( current_state != STATE_OPENING && client.connected() ) {
			current_state = STATE_OPENING;
			client.publish("garage/status", "opening", true);
		}
	} else

	// Up LED is rapid flashing (4+ changes)
	if ( upChanges >= 5 && downChanges == 0 ) {
		status = "ERROR\n";
		if ( current_state != STATE_ERROR && client.connected() ) {
			current_state = STATE_ERROR;
			client.publish("garage/status", "Power Recovered", true);
		}
	} else

	// Both LEDs are rapid flashing (4+ changes)
	if ( upChanges >= 5 && downChanges >= 5 ) {
		status = "ERROR\n";
		if ( current_state != STATE_ERROR && client.connected() ) {
			current_state = STATE_ERROR;
			client.publish("garage/status", "error", true);
		}

	} else {
		status = "UNKNOWN\n";
	}

	Serial.print("Status: "); Serial.println(status);
	//status += "upChanges: "; status += upChanges; status += "\n";
	//status += "downChanges: "; status += downChanges; status += "\n";
	//status += "Completed time: "; status += millis();
	upChanges = 0;
	downChanges = 0;
}

void callback(char* topic, byte* payload, unsigned int length) {
	String newTopic = topic;
	payload[length] = '\0';
	String newPayload = String((char *)payload);
	newPayload.toLowerCase();

	// Check if we got a command
	if ( newTopic == "garage/commands" ) {
		if ( newPayload == "open" && current_state == STATE_CLOSED ) {
			pulse_relay();
		}

		if ( newPayload == "close" && current_state == STATE_OPEN ) {
			pulse_relay();
		}

	}

	update_status += millis();
	update_status += F(": MQTT Message received - Topic: ");
	update_status += newTopic;
	update_status += F(" - Payload: ");
	update_status += newPayload;
	update_status += "\n";
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

void pulse_relay() {
	update_status += "Pulsing Relay\n";
	digitalWrite(PIN_RELAY, RELAY_LATCH);
	delay(500);
	digitalWrite(PIN_RELAY, RELAY_RELEASE);
}

void handle_root() {
	String ip_addr = "";
	for (auto a : addrList) {
		ip_addr += a.toString().c_str();
		ip_addr += "<br>";
	}
	String webpage = String("<!DOCTYPE HTML>") +
		F("<head><title>Garage Door</title></head>") +
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
		F("<tr><td>Up LED</td><td bgcolor='#") + ( digitalRead(PIN_UP_LED) ? "00FF00" : "FFFFFF" ) + F("'></td></tr>\n") +
		F("<tr><td>Down LED</td><td bgcolor='#") + ( digitalRead(PIN_DOWN_LED) ? "FF0000" : "FFFFFF" ) + F("'></td></tr>\n") +
		F("<tr><td>Door Status</td><td>") + status + F("</td></tr>\n") +
		F("</table>") +
		F("<br>Update Status:<br><pre>") + update_status + F("</pre>\n<br>") +
		F("- <a href=\"/reboot\">Reboot</a><br>\n") +
		F("- <a href=\"/update\">Update</a><br>\n") +
		F("<br><font size=\"-1\">") + ESP.getFullVersion() + F("</font></body></html>");
	httpServer.sendHeader("Refresh", "10");
	httpServer.send(200, "text/html", webpage);
}

void reboot() {
	String webpage = F("<html><head><meta http-equiv=\"refresh\" content=\"10;url=/\"></head><body>Rebooting</body></html>");
	httpServer.send(200, "text/html", webpage);
	httpServer.handleClient();
	delay(100);
	ESP.restart();
}
