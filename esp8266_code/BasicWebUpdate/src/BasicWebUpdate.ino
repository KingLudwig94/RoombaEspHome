#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
ADC_MODE(ADC_VCC);
 
// USER CONFIGURED SECTION START //
const char* ssid = "YOURSSID";
const char* password = "YOURPASSWORD";
 
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
 
void setup() {
	Serial.begin(115200);
	WiFi.mode(WIFI_STA);
	WiFi.hostname(WiFi.macAddress());
	WiFi.begin(ssid, password);

	httpServer.on("/", handle_root);
	httpServer.on("/reboot", reboot);
	httpUpdater.setup(&httpServer);
	httpServer.begin();
}

void loop() {
	httpServer.handleClient();
	delay(10);
}

void handle_root() {
	String webpage = String("<!DOCTYPE HTML>") +
		"<head><title>WebFlash Bootloader</title></head>\n" +
		"<body><table><tr><th colspan='2'>ESP8266 Infomation</th></tr>\n" +
		"<tr><td>MAC Address:</td><td>" + WiFi.macAddress().c_str() + "</td></tr>\n" +
		"<tr><td>CPU Speed:</td><td>" + ESP.getCpuFreqMHz() + " MHz</td></tr>\n" +
		"<tr><td>CPU ID:</td><td>" + ESP.getChipId() + "</td></tr>\n" +
		"<tr><td>Vcc:</td><td>" + ESP.getVcc()  + " mV</td></tr>\n" +
		"<tr><td>Flash Chip ID:</td><td>" + ESP.getFlashChipId() + "</td></tr>\n" +
		"<tr><td>Flash Size:</td><td>" + ESP.getFlashChipSize() + " bytes</td></tr>\n" +
		"<tr><td>Flash Real Size:</td><td>" + ESP.getFlashChipRealSize() + " bytes</td></tr>\n" +
		"<tr><td>Flash Speed:</td><td>" + (ESP.getFlashChipSpeed() / 1000000) + " Mhz</td></tr>\n" +
		"<tr><td>Flash Mode:</td><td>" +  (
			ESP.getFlashChipMode() == FM_QIO ? "QIO" :
			ESP.getFlashChipMode() == FM_QOUT ? "QOUT" :
			ESP.getFlashChipMode() == FM_DIO ? "DIO" :
			ESP.getFlashChipMode() == FM_DOUT ? "DOUT" :
			"UNKNOWN") + "</td></tr>\n" +
		"</table>\n" +
		"<br>- <a href=\"/reboot\">Reboot</a><br>- <a href=\"/update\">Update Firmware</a>" +
		"</pre><br><br><font size=\"-1\">" + ESP.getFullVersion() + "</font></body></html>";
	httpServer.sendHeader("Refresh", "10");
	httpServer.send(200, "text/html", webpage);
}

void reboot() {
	String webpage = "<html><head><meta http-equiv=\"refresh\" content=\"10;url=/\"></head><body>Rebooting</body></html>";
	httpServer.send(200, "text/html", webpage);
	httpServer.handleClient();
	delay(100);
	ESP.restart();
}

