#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266httpUpdate.h>

#define PIN_PULSE	D5
#define PIN_DIALING	D6
#define PIN_OFFHOOK	D7

#define PIN_MOTOR_1 D1
#define PIN_MOTOR_2 D2

// USER CONFIGURED SECTION START //
const char* ssid = "YOURSSID";
const char* password = "YOURPASSWORD";

ESP8266WebServer httpServer(80);

String update_status;
uint8_t stackptr = 0;
uint8_t Number[20]; // Max length of any number
uint16_t DIAL_TIMEOUT = 3000; // Time in ms to wait for more digits

void setup() {
	Serial.begin(115200);

	// Set up the GPIO pins.
	pinMode(PIN_DIALING, INPUT_PULLUP);
	pinMode(PIN_PULSE, INPUT_PULLUP);
	pinMode(PIN_OFFHOOK, INPUT_PULLUP);
	pinMode(PIN_MOTOR_1, OUTPUT);
	pinMode(PIN_MOTOR_2, OUTPUT);

	// Reset the ringer...
	digitalWrite(PIN_MOTOR_1, LOW);
	digitalWrite(PIN_MOTOR_2, LOW);

	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);
}

void loop() {
	if ( digitalRead(PIN_OFFHOOK) == LOW ) {
		Serial.println("Pickup detected...");
		uint16_t timeout = 0;
		// Only loop here while we're off hook.
		while ( digitalRead(PIN_OFFHOOK) == LOW && timeout < DIAL_TIMEOUT ) {
			// Now we have to figure out what number is being dialed.
			int8_t dialed_number = -1;

			while ( digitalRead(PIN_DIALING) == LOW && digitalRead(PIN_OFFHOOK) == LOW ) {
				timeout = 0;
				if ( digitalRead(PIN_PULSE) == LOW ) {
					delay(5);
					if ( digitalRead(PIN_PULSE) == HIGH ) {
						dialed_number++;
					}
				}
			}
			if ( dialed_number > -1 ) {
				dialed_number++;
				if ( dialed_number == 10 ) {
					dialed_number = 0;
				}
				Number[stackptr] = dialed_number;
				stackptr++;
			}
			timeout = timeout + 10;
			delay(10);
		}

		// Handle a hangup at this point.
		if ( digitalRead(PIN_OFFHOOK) == HIGH ) {
			Serial.println("Hangup detected");
			return;
		}

		if ( stackptr != 0 ) {
			Serial.print("Dialing complete. Received: ");
			for (int i = 0; i < stackptr; i++) {
				Serial.print(Number[i]);
			}
			Serial.println("");
		} else {
			Serial.println("No number dialed...");
		}

		// While we stay offhook, loop in here.
		while ( digitalRead(PIN_OFFHOOK) == LOW ) {
			delay(100);
		}
		Serial.println("Hangup detected");

	} else {
		// Reset the number stackptr ready for next number.
		stackptr = 0;
		Ringer();
	}

	delay(50);
}

void Ringer() {
	while ( digitalRead(PIN_OFFHOOK) == HIGH ) {
		if ( digitalRead(PIN_MOTOR_1) == LOW ) {
			digitalWrite(PIN_MOTOR_1, HIGH);
			digitalWrite(PIN_MOTOR_2, LOW);
		} else {
			digitalWrite(PIN_MOTOR_1, LOW);
			digitalWrite(PIN_MOTOR_2, HIGH);
		}
		delay(25);
	}
	return;
}
