#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>

// FastLED DMA fork from: https://github.com/coryking/FastLED/
// This MUST use the RX pin (aka GPIO 3) on the ESP8266 to function.
#define FASTLED_ESP8266_DMA
#define FASTLED_ALLOW_INTERRUPTS 0
#include <FastLED.h>

// Comment out the following line to build without HTTP autoupdate.
#define WITH_AUTOUPDATE
#ifdef WITH_AUTOUPDATE
#include <Ticker.h>
#include <../../autoupdate.h>
// Configure the ticker timer.
Ticker updateTicker;
bool run_update = false;
#endif

/************ MQTT Information (CHANGE THESE FOR YOUR SETUP) ******************/
const char* mqtt_server = "MQTTIP";
const int mqtt_port = 1883;
const char *mqtt_user = NULL;
const char *mqtt_pass = NULL;

#define MILLION 1000000
WiFiEventHandler mConnectHandler, mDisConnectHandler, mGotIpHandler;

/************ OTA Updates ******************/
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
#ifdef WITH_AUTOUPDATE
String update_status;
#else
String update_status = "Not built with WITH_AUTOUPDATE enabled.";
#endif

/************* MQTT TOPICS (change these topics as you wish)	**************************/
const char* light_state_topic = "lights/bedroom";
const char* light_set_topic = "lights/bedroom/set";

const char* on_cmd = "ON";
const char* off_cmd = "OFF";
const char* effect = "solid";
long lastReconnectAttempt = 0;
String effectString = "solid";
String oldeffectString = "solid";

/****************************************FOR JSON***************************************/
const int BUFFER_SIZE = JSON_OBJECT_SIZE(10);
#define MQTT_MAX_PACKET_SIZE 512

/*********************************** FastLED Defintions ********************************/
#define NUM_LEDS	46
#define CHIPSET	 WS2812B
#define COLOR_ORDER GRB
struct CRGB leds[NUM_LEDS];

byte realRed = 0;
byte realGreen = 0;
byte realBlue = 0;

byte red = 255;
byte green = 255;
byte blue = 255;
byte brightness = 255;

/******************************** GLOBALS for fade/flash *******************************/
bool stateOn = false;
bool startFade = false;
bool inFade = false;
unsigned long lastLoop = 0;
int transitionTime = 0;
int effectSpeed = 0;
int loopCount = 0;
int stepR, stepG, stepB;
int redVal, grnVal, bluVal;

/********************************** GLOBALS for EFFECTS ******************************/
//RAINBOW
uint8_t thishue = 0;											// Starting hue value.
uint8_t deltahue = 10;

//CANDYCANE
CRGBPalette16 currentPalettestriped; //for Candy Cane
CRGBPalette16 gPal; //for fire

//NOISE
static uint16_t dist;		 // A random number for our noise generator.
uint16_t scale = 30;			// Wouldn't recommend changing this on the fly, or the animation will be really blocky.
uint8_t maxChanges = 48;		// Value for blending between palettes.
CRGBPalette16 targetPalette(OceanColors_p);
CRGBPalette16 currentPalette(CRGB::Black);

//TWINKLE
#define DENSITY	 80

//RIPPLE
uint8_t colour;											// Ripple colour is randomized.
int center = 0;											// Center of the current ripple.
int step = -1;											// -1 is the initializing step.
uint8_t myfade = 255;									// Starting brightness.
#define maxsteps 16										// Case statement wouldn't allow a variable.
uint8_t bgcol = 0;										// Background colour rotates.
int thisdelay = 20;										// Standard delay value.

//DOTS
uint8_t	 count =	 0;									// Count up to 255 and then reverts to 0
uint8_t fadeval = 224;									// Trail behind the LED's. Lower => faster fade.
uint8_t bpm = 30;

//LIGHTNING
uint8_t frequency = 50;									// controls the interval between strikes
uint8_t flashes = 8;									//the upper limit of flashes per strike
unsigned int dimmer = 1;
uint8_t ledstart;										// Starting location of a flash
uint8_t ledlen;
int lightningcounter = 0;

//FUNKBOX
int idex = 0;											//-LED INDEX (0 to NUM_LEDS-1
int TOP_INDEX = int(NUM_LEDS / 2);
int thissat = 255;										//-FX LOOPS DELAY VAR
uint8_t thishuepolice = 0;
int antipodal_index(int i) {
	int iN = i + TOP_INDEX;
	if (i >= TOP_INDEX) {
		iN = ( i + TOP_INDEX ) % NUM_LEDS;
	}
	return iN;
}

//FIRE
#define COOLING	55
#define SPARKING 120
bool gReverseDirection = false;

//BPM
uint8_t gHue = 0;

WiFiClient espClient;
PubSubClient client(espClient);

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
	update_status += ": Station connected, IP: ";
	update_status += WiFi.localIP().toString();
	update_status += "\n";
	#ifdef WITH_AUTOUPDATE
	// Check for update in 10 seconds time.
	updateTicker.once(10, []() { run_update = true; });
	#endif
}

/********************************** START SETUP*****************************************/
void setup() {
	Serial.begin(115200);
	mConnectHandler = WiFi.onStationModeConnected(onConnected);
	mDisConnectHandler = WiFi.onStationModeDisconnected(onDisconnect);
	mGotIpHandler = WiFi.onStationModeGotIP(onGotIP);

	// Set power saving for device. WIFI_LIGHT_SLEEP is buggy, so use WIFI_MODEM_SLEEP
	WiFi.setSleepMode(WIFI_MODEM_SLEEP);
	WiFi.setOutputPower(19);	// 10dBm == 10mW, 14dBm = 25mW, 17dBm = 50mW, 20dBm = 100mW

	// Start connecting to wifi.
	WiFiManager WiFiManager;
	if (!WiFiManager.autoConnect()) {
		ESP.reset();
		delay(1000);
	}

	FastLED.addLeds<CHIPSET, RX, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalSMD5050);
	FastLED.setMaxRefreshRate(90);
	FastLED.setDither( 1 );

	setupStripedPalette( CRGB::Red, CRGB::Red, CRGB::White, CRGB::White ); //for CANDY CANE
	gPal = HeatColors_p; //for FIRE

	httpServer.on("/", handle_root);
	httpServer.on("/reboot", reboot);
	httpUpdater.setup(&httpServer);
	httpServer.begin();

	client.setServer(mqtt_server, mqtt_port);
	client.setCallback(callback);
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
		F("<head><title>Bedroom LEDs</title></head>") +
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
		F("<tr><td>Current Effect</td><td>") + effectString + F("</td></tr>\n") +
		F("<tr><td>Power</td><td>") + ( stateOn ? "On" : "Off" ) + F("</td></tr>\n") +
		F("</table>\n") +
		F("<br>- <a href=\"/reboot\">Reboot</a><br>- <a href=\"/update\">Update</a><br><br>Update Status:<br><pre>") + update_status +
		F("</pre><font size=\"-1\">") + ESP.getFullVersion() + F("</font></body></html>");
		httpServer.sendHeader("Refresh", "10");
		httpServer.send(200, "text/html", webpage);
}

void reboot() {
	String webpage = "<html><head><meta http-equiv=\"refresh\" content=\"10;url=/\"></head><body>Rebooting</body></html>";
	httpServer.send(200, "text/html", webpage);
	client.publish("lights/bedroom/status", "Rebooting");
	httpServer.handleClient();
	client.disconnect();
	httpServer.handleClient();
	delay(250);
	httpServer.handleClient();
	ESP.restart();
}

/*
	SAMPLE PAYLOAD:
	{
	"brightness": 120,
	"color": {
		"r": 255,
		"g": 100,
		"b": 100
	},
	"flash": 2,
	"transition": 5,
	"state": "ON"
	}
*/

/********************************** START CALLBACK*****************************************/
void callback(char* topic, byte* payload, unsigned int length) {
	char message[length + 1];
	for (unsigned int i = 0; i < length; i++) {
		message[i] = (char)payload[i];
	}
	message[length] = '\0';

	if (!processJson(message)) {
		return;
	}

	if (stateOn) {
		realRed = map(red, 0, 255, 0, brightness);
		realGreen = map(green, 0, 255, 0, brightness);
		realBlue = map(blue, 0, 255, 0, brightness);
	}
	else {
		realRed = 0;
		realGreen = 0;
		realBlue = 0;
	}

	startFade = true;
	inFade = false; // Kill the current fade

	sendState();
}



/********************************** START PROCESS JSON*****************************************/
bool processJson(char* message) {
	StaticJsonDocument<BUFFER_SIZE> root;

	auto error = deserializeJson(root, message);
	if (error) {
		return false;
	}

	if (root.containsKey("state")) {
		if (strcmp(root["state"], on_cmd) == 0) {
			stateOn = true;
		}
		else if (strcmp(root["state"], off_cmd) == 0) {
			fadeToBlackBy( leds, NUM_LEDS, 25);
			effectString = "solid";
			stateOn = false;
		}
	}

	if (root.containsKey("color")) {
		red = root["color"]["r"];
		green = root["color"]["g"];
		blue = root["color"]["b"];
	}

	if (root.containsKey("color_temp")) {
		//temp comes in as mireds, need to convert to kelvin then to RGB
		int color_temp = root["color_temp"];
		unsigned int kelvin	= MILLION / color_temp;
		temp2rgb(kelvin);
	}

	if (root.containsKey("brightness")) {
		brightness = root["brightness"];
	}

	if (root.containsKey("effect")) {
		effect = root["effect"];
		effectString = effect;
	}

	if (root.containsKey("transition")) {
		transitionTime = root["transition"];
	} else {
		transitionTime = 5;
	}

	return true;
}

/********************************** START SEND STATE*****************************************/
void sendState() {
	DynamicJsonDocument doc(BUFFER_SIZE);

	doc["state"] = (stateOn) ? on_cmd : off_cmd;
	doc["brightness"] = brightness;
	doc["effect"] = effectString.c_str();

	JsonObject color = doc.createNestedObject("color");
	color["r"] = red;
	color["g"] = green;
	color["b"] = blue;

	String output = "";
	serializeJson(doc, output);

	client.publish(light_state_topic, output.c_str(), true);
}


/********************************** START MAIN LOOP*****************************************/
void loop() {
	httpServer.handleClient();
	yield();

	#ifdef WITH_AUTOUPDATE
	if ( run_update ) {
		update_status = checkForUpdate();
		updateTicker.once(8 * 60 * 60, []() { run_update = true; });
		run_update = false;
	}
	#endif

	if ( ! client.loop() && WiFi.status() == WL_CONNECTED ) {
		long now = millis();
		if (now - lastReconnectAttempt > 5000) {
			lastReconnectAttempt = now;
			update_status += millis();
			update_status += ": Attempting to connect to MQTT Server\n";
			if (client.connect(WiFi.macAddress().c_str(), mqtt_user, mqtt_pass, "lights/bedroom/status", 0, 0, "MQTT Disconnected")) {
				update_status += millis();
				update_status += ": Connected to MQTT Server\n";
				client.subscribe(light_set_topic);
			}
		}
	}

	if ( stateOn ) {
		EVERY_N_MILLISECONDS(10) {
			FastLED_Timer();
		}

		EVERY_N_SECONDS(10) {
			random16_add_entropy( random8());
			targetPalette = CRGBPalette16(CHSV(random8(), 255, random8(128, 255)), CHSV(random8(), 255, random8(128, 255)), CHSV(random8(), 192, random8(128, 255)), CHSV(random8(), 255, random8(128, 255)));
		}
		FastLED.show();
	}

	if (startFade) {
		loopCount = 0;
		inFade = true;
	}

	if (inFade) {
		startFade = false;
		if ( loopCount <= 120 ) {
			CRGB arrTargetColor(realRed, realGreen, realBlue);
			fadeTowardColor( leds, NUM_LEDS, arrTargetColor, 8);
			loopCount++;
			FastLED.show();
		} else {
			inFade = false;
		}
	}

	if ( ! stateOn && ! inFade ) {
		FastLED.show();
		FastLED.delay(50);
	}
	yield();
}

void FastLED_Timer() {
	if ( effectString == "bpm" ) {
		uint8_t BeatsPerMinute = 62;
		CRGBPalette16 palette = PartyColors_p;
		uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
		for ( int i = 0; i < NUM_LEDS; i++) { //9948
			leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
		}
		gHue++;
	} else

	if (effectString == "candy cane") {
		static uint8_t startIndex = 0;
		startIndex = startIndex + 1; /* higher = faster motion */
		fill_palette( leds, NUM_LEDS, startIndex, 16, currentPalettestriped, 255, LINEARBLEND);
	} else

	if ( effectString == "confetti" ) {
		fadeToBlackBy( leds, NUM_LEDS, 25);
		int pos = random16(NUM_LEDS);
		leds[pos] += CRGB(realRed + random8(64), realGreen, realBlue);
	} else

	if ( effectString == "cyclon rainbow" ) {
		static uint8_t hue = 0;
		// First slide the led in one direction
		for (int i = 0; i < NUM_LEDS; i++) {
			// Set the i'th led to red
			leds[i] = CHSV(hue++, 255, 255);
			// Show the leds
			FastLED.show();
			// now that we've shown the leds, reset the i'th led to black
			// leds[i] = CRGB::Black;
			fadeall();
			// Wait a little bit before we loop around and do it again
			FastLED.delay(10);
		}
		for (int i = (NUM_LEDS) - 1; i >= 0; i--) {
			// Set the i'th led to red
			leds[i] = CHSV(hue++, 255, 255);
			// Show the leds
			FastLED.show();
			// now that we've shown the leds, reset the i'th led to black
			// leds[i] = CRGB::Black;
			fadeall();
			// Wait a little bit before we loop around and do it again
			FastLED.delay(10);
		}
	} else

	if ( effectString == "dots" ) {
		uint8_t inner = beatsin8(bpm, NUM_LEDS / 4, NUM_LEDS / 4 * 3);
		uint8_t outer = beatsin8(bpm, 0, NUM_LEDS - 1);
		uint8_t middle = beatsin8(bpm, NUM_LEDS / 3, NUM_LEDS / 3 * 2);
		leds[middle] = CRGB::Purple;
		leds[inner] = CRGB::Blue;
		leds[outer] = CRGB::Aqua;
		nscale8(leds, NUM_LEDS, fadeval);
	} else

	if ( effectString == "fire" ) {
		Fire2012WithPalette();
	} else

	if ( effectString == "glitter" ) {
		fadeToBlackBy( leds, NUM_LEDS, 20);
		addGlitterColor(80, realRed, realGreen, realBlue);
	} else

	if ( effectString == "juggle" ) {
		fadeToBlackBy(leds, NUM_LEDS, 20);
		for (int i = 0; i < 8; i++) {
			leds[beatsin16(i + 7, 0, NUM_LEDS - 1	)] |= CRGB(realRed, realGreen, realBlue);
		}
	} else

	if ( effectString == "lightning" ) {
		fill_solid(leds, NUM_LEDS, 0);
		ledstart = random8(NUM_LEDS);						// Determine starting location of flash
		ledlen = random8(NUM_LEDS-ledstart);
		for (int flashCounter = 0; flashCounter < random8(3,flashes); flashCounter++) {
			if(flashCounter == 0) dimmer = 5;				// the brightness of the leader is scaled down by a factor of 5
			else dimmer = random8(1,3);					// return strokes are brighter than the leader
			fill_solid(leds+ledstart,ledlen,CHSV(255, 0, 255/dimmer));
			FastLED.show();							// Show a section of LED's
			delay(random8(4,10));						// each flash only lasts 4-10 milliseconds
			fill_solid(leds+ledstart,ledlen,CHSV(255,0,0));			// Clear the section of LED's
			FastLED.show();
			if (flashCounter == 0) delay (150);				// longer delay until next flash after the leader
			delay(50+random8(100));						// shorter delay between strokes
		}
		delay(random8(frequency)*100);
		return;
	} else

	if ( effectString == "noise" ) {
		for (int i = 0; i < NUM_LEDS; i++) {
			uint8_t index = inoise8(i * scale, dist + i * scale) % 255;
			leds[i] = ColorFromPalette(currentPalette, index, 255, LINEARBLEND);
		}
		dist += beatsin8(10, 1, 4);
		nblendPaletteTowardPalette(currentPalette, targetPalette, maxChanges);	// FOR NOISE ANIMATION
	} else

	if ( effectString == "police all" ) {
		idex++;
		if (idex >= NUM_LEDS) {
			idex = 0;
		}
		int idexR = idex;
		int idexB = antipodal_index(idexR);
		int thathue = (thishuepolice + 160) % 255;
		leds[idexR] = CHSV(thishuepolice, thissat, 255);
		leds[idexB] = CHSV(thathue, thissat, 255);
	} else

	if (effectString == "police one") {
		idex++;
		if (idex >= NUM_LEDS) {
			idex = 0;
		}
		int idexR = idex;
		int idexB = antipodal_index(idexR);
		int thathue = (thishuepolice + 160) % 255;
		for (int i = 0; i < NUM_LEDS; i++ ) {
			if (i == idexR) {
				leds[i] = CHSV(thishuepolice, thissat, 255);
			}
			else if (i == idexB) {
				leds[i] = CHSV(thathue, thissat, 255);
			}
			else {
				leds[i] = CHSV(0, 0, 0);
			}
		}
	} else

	if ( effectString == "rainbow" ) {
		// FastLED's built-in rainbow generator
		thishue++;
		fill_rainbow(leds, NUM_LEDS, thishue, deltahue);
	} else

	if ( effectString == "rainbow with glitter" ) {
		thishue++;
		fill_rainbow(leds, NUM_LEDS, thishue, deltahue);
		addGlitter(25);
	} else

	if ( effectString == "ripple" ) {
		for (int i = 0; i < NUM_LEDS; i++) leds[i] = CHSV(bgcol++, 255, 15);	// Rotate background colour.
		switch (step) {
			case -1:						// Initialize ripple variables.
				center = random(NUM_LEDS);
				colour = random8();
				step = 0;
				break;
			case 0:
				leds[center] = CHSV(colour, 255, 255);		// Display the first pixel of the ripple.
				step ++;
				break;
			case maxsteps:						// At the end of the ripples.
				step = -1;
				break;
			default:						// Middle of the ripples.
				leds[(center + step + NUM_LEDS) % NUM_LEDS] += CHSV(colour, 255, myfade / step * 2);	 // Simple wrap from Marc Miller
				leds[(center - step + NUM_LEDS) % NUM_LEDS] += CHSV(colour, 255, myfade / step * 2);
				step ++;					// Next step.
				break;
		}
	} else

	if ( effectString == "sinelon" ) {
		fadeToBlackBy( leds, NUM_LEDS, 20);
		int pos = beatsin16(13, 0, NUM_LEDS - 1);
		leds[pos] += CRGB(realRed, realGreen, realBlue);
	} else

	if ( effectString == "twinkle" ) {
		const CRGB lightcolor(8, 7, 1);
		for ( int i = 0; i < NUM_LEDS; i++) {
			if ( !leds[i]) continue; // skip black pixels
			if ( leds[i].r & 1) { // is red odd?
				leds[i] -= lightcolor; // darken if red is odd
			} else {
				leds[i] += lightcolor; // brighten if red is even
			}
		}
		if ( random8() < DENSITY) {
			int j = random16(NUM_LEDS);
			if ( !leds[j] ) leds[j] = lightcolor;
		}
	}

}

// Helper function that blends one uint8_t toward another by a given amount
void nblendU8TowardU8( uint8_t& cur, const uint8_t target, uint8_t amount) {
	if( cur == target) return;

	if( cur < target ) {
		uint8_t delta = target - cur;
		delta = scale8_video( delta, amount);
		cur += delta;
	} else {
		uint8_t delta = cur - target;
		delta = scale8_video( delta, amount);
		cur -= delta;
	}
}

// Blend one CRGB color toward another CRGB color by a given amount.
// Blending is linear, and done in the RGB color space.
// This function modifies 'cur' in place.
CRGB fadeTowardColor( CRGB& cur, const CRGB& target, uint8_t amount) {
	nblendU8TowardU8( cur.red,  target.red,  amount);
	nblendU8TowardU8( cur.green, target.green, amount);
	nblendU8TowardU8( cur.blue, target.blue, amount);
	return cur;
}

// Fade an entire array of CRGBs toward a given background color by a given amount
// This function modifies the pixel array in place.
void fadeTowardColor( CRGB* L, uint16_t N, const CRGB& bgColor, uint8_t fadeAmount) {
	for( uint16_t i = 0; i < N; i++) {
		fadeTowardColor( L[i], bgColor, fadeAmount);
	}
}

/**************************** START STRIPLED PALETTE *****************************************/
void setupStripedPalette( CRGB A, CRGB AB, CRGB B, CRGB BA) {
	currentPalettestriped = CRGBPalette16(
		A, A, A, A, A, A, A, A, B, B, B, B, B, B, B, B
//		A, A, A, A, A, A, A, A, B, B, B, B, B, B, B, B
	);
}

/********************************** START FADE************************************************/
void fadeall() {
	for (int i = 0; i < NUM_LEDS; i++) {
		leds[i].nscale8(250);	//for CYCLon
	}
}

/********************************** START FIRE **********************************************/
void Fire2012WithPalette() {
	// Array of temperature readings at each simulation cell
	static byte heat[NUM_LEDS];

	// Step 1.	Cool down every cell a little
	for ( int i = 0; i < NUM_LEDS; i++) {
		heat[i] = qsub8( heat[i],	random8(0, ((COOLING * 10) / NUM_LEDS) + 2));
	}

	// Step 2.	Heat from each cell drifts 'up' and diffuses a little
	for ( int k = NUM_LEDS - 1; k >= 2; k--) {
		heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
	}

	// Step 3.	Randomly ignite new 'sparks' of heat near the bottom
	if ( random8() < SPARKING ) {
		int y = random8(7);
		heat[y] = qadd8( heat[y], random8(160, 255) );
	}

	// Step 4.	Map from heat cells to LED colors
	for ( int j = 0; j < NUM_LEDS; j++) {
		// Scale the heat value from 0-255 down to 0-240
		// for best results with color palettes.
		byte colorindex = scale8( heat[j], 240);
		CRGB color = ColorFromPalette( gPal, colorindex);
		int pixelnumber;
		if ( gReverseDirection ) {
			pixelnumber = (NUM_LEDS - 1) - j;
		} else {
			pixelnumber = j;
		}
		leds[pixelnumber] = color;
	}
}

/********************************** START ADD GLITTER *********************************************/
void addGlitter( fract8 chanceOfGlitter ) {
	if ( random8() < chanceOfGlitter) {
		leds[ random16(NUM_LEDS) ] += CRGB::White;
	}
}

/********************************** START ADD GLITTER COLOR ****************************************/
void addGlitterColor( fract8 chanceOfGlitter, int red, int green, int blue ) {
	if ( random8() < chanceOfGlitter) {
		leds[ random16(NUM_LEDS) ] += CRGB(red, green, blue);
	}
}

void temp2rgb(unsigned int kelvin) {
	int tmp_internal = kelvin / 100.0;

	// red
	if (tmp_internal <= 66) {
		red = 255;
	} else {
		float tmp_red = 329.698727446 * pow(tmp_internal - 60, -0.1332047592);
		if (tmp_red < 0) {
			red = 0;
		} else if (tmp_red > 255) {
			red = 255;
		} else {
			red = tmp_red;
		}
	}

	// green
	if (tmp_internal <=66){
		float tmp_green = 99.4708025861 * log(tmp_internal) - 161.1195681661;
		if (tmp_green < 0) {
			green = 0;
		} else if (tmp_green > 255) {
			green = 255;
		} else {
			green = tmp_green;
		}
	} else {
		float tmp_green = 288.1221695283 * pow(tmp_internal - 60, -0.0755148492);
		if (tmp_green < 0) {
			green = 0;
		} else if (tmp_green > 255) {
			green = 255;
		} else {
			green = tmp_green;
		}
	}

	// blue
	if (tmp_internal >=66) {
		blue = 255;
	} else if (tmp_internal <= 19) {
		blue = 0;
	} else {
		float tmp_blue = 138.5177312231 * log(tmp_internal - 10) - 305.0447927307;
		if (tmp_blue < 0) {
			blue = 0;
		} else if (tmp_blue > 255) {
			blue = 255;
		} else {
			blue = tmp_blue;
		}
	}
}
