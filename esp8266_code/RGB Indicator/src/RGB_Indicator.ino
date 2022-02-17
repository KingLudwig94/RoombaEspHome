#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <Ticker.h>
#include <FastLED.h>

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

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
WiFiEventHandler mConnectHandler, mDisConnectHandler, mGotIpHandler;

// Define our FastLED configuration
#define NUM_LEDS			13
#define DATA_PIN			D8
#define CHIPSET				WS2812B
#define COLOR_ORDER		GRB
struct CRGB leds[NUM_LEDS];

// Effect Queue
struct effect {
	uint8_t effect_type;
	CRGB colour;
	uint8_t repeat = 0;
	bool skip = true;
};
#define QUEUE_LENGTH	20
struct effect effect_queue[QUEUE_LENGTH];
uint8_t effect_queue_ptr = 0;

void effect_queue_add(uint8_t effect, CRGB colour) {
	effect_queue_add(effect, colour, 0);
}

void effect_queue_add(uint8_t effect, CRGB colour, uint8_t repeat) {
	// Look for an empty spot...
	for (int i = 0; i < QUEUE_LENGTH; i++) {
		if ( effect_queue[i].skip == true ) {
			effect_queue[i].effect_type = effect;
			effect_queue[i].colour = colour;
			effect_queue[i].repeat = repeat;
			effect_queue[i].skip = false;
			effect_queue_ptr = i;
			return;
		}
	}
}

/************ WiFi event handlers ******************/
void onConnected(const WiFiEventStationModeConnected& event){
	effect_queue_add(0, CRGB(0,255,0));
	Serial.print(millis());
	Serial.println(": Connected to AP.");
}

void onDisconnect(const WiFiEventStationModeDisconnected& event){
	effect_queue_add(0, CRGB(255,0,0));
	Serial.print(millis());
	Serial.println(": Station disconnected");
}

void onGotIP(const WiFiEventStationModeGotIP& event){
	effect_queue_add(0, CRGB(0,255,0));
	Serial.print(millis());
	Serial.print(": Station connected, IP: ");
	Serial.println(WiFi.localIP().toString());
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

/********************************** START SETUP*****************************************/
void setup() {
	Serial.begin(115200);
	Serial.println("");

	// Set up FastLED and set everything to off...
	FastLED.addLeds<CHIPSET, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
	fill_solid(leds, NUM_LEDS, 0);
	FastLED.show();

	// Set up wifi event handlers.
	mConnectHandler = WiFi.onStationModeConnected(onConnected);
	mDisConnectHandler = WiFi.onStationModeDisconnected(onDisconnect);
	mGotIpHandler = WiFi.onStationModeGotIP(onGotIP);

	// Set power saving for device. WIFI_LIGHT_SLEEP is buggy, so use WIFI_MODEM_SLEEP
	//WiFi.setSleepMode(WIFI_LIGHT_SLEEP);
	WiFi.setSleepMode(WIFI_MODEM_SLEEP);
	//WiFi.setSleepMode(WIFI_NONE_SLEEP);
	WiFi.setOutputPower(18);	// 10dBm == 10mW, 14dBm = 25mW, 17dBm = 50mW, 20dBm = 100mW

	// Start connecting to wifi.
	WiFiManager WiFiManager;
	if (!WiFiManager.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    ESP.reset();
    delay(1000);
  }

	// Connected to wifi, so start up our http server.
	httpServer.on("/", handle_root);
	httpServer.on("/json", handle_json);
	httpServer.on("/reboot", reboot);
	httpUpdater.setup(&httpServer);
	httpServer.begin();

	#ifdef WITH_AUTOUPDATE
	Serial.println("Autoupdate enabled at compile time...");
	#endif
	Serial.println("Ready!");
}

/********************************** START MAIN LOOP*****************************************/
void loop() {
	// Feed the watchdog to try and avoid cruddy resets.
	ESP.wdtFeed();
	httpServer.handleClient();

	#ifdef WITH_AUTOUPDATE
	if ( run_update ) {
		Serial.println(checkForUpdate());
		updateTicker.attach(8 * 60 * 60, UpdateTimer);
		run_update = false;
		Serial.print(millis());
		Serial.println(": Finished auto-update check...");
	}
	#endif

	// Scan the effect_queue for something to do...
	for (int i = 0; i < QUEUE_LENGTH; i++) {
		if ( effect_queue[i].skip == true ) {
			continue;
		}

		Serial.print("Processing pointer: ");
		Serial.println(i);
		// Fade effect.
		if ( effect_queue[i].effect_type == 0 ) {
			colorFade(effect_queue[i].colour);
		}
		if ( effect_queue[i].effect_type == 1 ) {
			colorWipe(effect_queue[i].colour);
		}
		if ( effect_queue[i].effect_type == 2 ) {
			colorFlash(effect_queue[i].colour);
		}

		if ( effect_queue[i].repeat == 0 ) {
			// Don't fire this event again.
			effect_queue[i].skip = true;
		} else {
			effect_queue[i].repeat--;
		}
	}

	// Do an adaptive sleep. If we've done no work (the effect_queue_ptr != 0), sleep longer...
	if ( effect_queue_ptr == 0 ) {
		delay(50);
	} else {
		delay(5);
	}
	effect_queue_ptr = 0;
}

void handle_root() {
	String webpage = "<html><head><meta http-equiv=\"refresh\" content=\"5\"></head><body><h3>RGB Indicator</h3>";
	webpage += "<table><tr><th colspan='2'><strong>WiFi Connection Details:</strong></th></tr><tr><td>SSID:</td><td>";
	webpage += WiFi.SSID();
	webpage += "</td></tr><tr><td>IP:</td><td>";
	webpage += WiFi.localIP().toString();
	webpage += "</td></tr><tr><td>RSSI:</td><td>";
	webpage += WiFi.RSSI();
	webpage += " dBm</td></tr><tr></table><br>";
	webpage += "<br>- <a href=\"/reboot\">Reboot</a><br><br><font size=\"-1\">";
	webpage += ESP.getFullVersion();
	webpage += "</font></body></html>";
	httpServer.send(200, "text/html", webpage);
}

void handle_json() {
	DynamicJsonDocument doc(256);
	Serial.println("JSON received: " + httpServer.arg("plain"));

	auto error = deserializeJson(doc, httpServer.arg("plain"));
	if (error) {
		httpServer.send( 400, "application/json", "{\"error\": \"parseObject() failed\"}");
		Serial.println("parseObject() failed");
		return;
	}

	if ( doc.containsKey("id") ) {
		uint8_t id = doc["id"];
		uint8_t repeat = 0;
		Serial.print("Adding effect ");
		Serial.print(id);
		Serial.println(" to the queue...");

		// Handle repeats of the same effect
		if ( doc.containsKey("repeat") ) {
			repeat = doc["repeat"];
			repeat--;
		}

		// Fade in and out of supplied colour
		// Example JSON packet: '{"id": 0, "colour": { r: 255, g: 0, b: 255} , repeat:2}'
		Serial.println("Running effect_queue_add()");
		CRGB colour = CRGB(doc["colour"]["r"], doc["colour"]["g"], doc["colour"]["b"]);
		effect_queue_add( id, colour, repeat );

		httpServer.send( 200, "text/json", "{success:true}" );
	} else {
		httpServer.send( 400, "application/json", "{\"error\": \"invalid JSON\"}");
		return;
	}
}

void reboot() {
	String webpage = "<html><head><meta http-equiv=\"refresh\" content=\"10;url=/\"></head><body>Rebooting</body></html>";
	httpServer.send(200, "text/html", webpage);
	httpServer.handleClient();
	delay(200);
	ESP.restart();
}


// Effect 0
void colorFade(CRGB c) {
	Serial.println("Starting colorFade...");
	for( uint8_t brightness = 0; brightness < 255; brightness++ ) {
		fill_solid(leds, NUM_LEDS, c);
		FastLED.setBrightness(brightness);
		FastLED.show();
		httpServer.handleClient();
		delay(2);
	}
	Serial.println("End colorFade...");
	fadeToBlack();
}

// Effect 1
void colorWipe(CRGB c) {
	Serial.println("Starting colorWipe...");
	FastLED.setBrightness(255);
	for( uint8_t i=0; i<NUM_LEDS; i++ ) {
		leds[i] = c;
		FastLED.show();
		httpServer.handleClient();
		delay(50);
	}
	Serial.println("End colorWipe...");
	fadeToBlack();
}

// Effect 2
void colorFlash(CRGB c) {
	Serial.println("Starting colorFlash...");
	FastLED.setBrightness(255);
	fill_solid(leds, NUM_LEDS, c);
	FastLED.show();
	for( uint8_t i=0; i > 50; i++ ) {
		httpServer.handleClient();
		delay(10);
	}
	fill_solid(leds, NUM_LEDS, 0);
	FastLED.show();
	Serial.println("End colorFlash...");
}

// Fade back to black.
void fadeToBlack() {
	Serial.println("Starting fadeToBlack...");
	for( uint8_t i=255; i > 0; i-- ) {
		FastLED.setBrightness(i);
		FastLED.show();
		httpServer.handleClient();
		delay(2);
	}
	fill_solid(leds, NUM_LEDS, 0);
	FastLED.show();
	Serial.println("End fadeToBlack...");
}
