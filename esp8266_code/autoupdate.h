#ifdef WITH_AUTOUPDATE
String checkForUpdate() {
	String update_status;
	update_status = millis();

	// Don't run if wifi isn't connected...
	if ( WiFi.status() != WL_CONNECTED ) {
		update_status += F(": Wifi not connected. Skipping update check\n");
		return update_status;
	}
	update_status += F(": Checking for update...\n");

	BearSSL::WiFiClientSecure UpdateClient;
	UpdateClient.setInsecure();
	t_httpUpdate_return result = ESPhttpUpdate.update(UpdateClient, F("https://lamp.crc.id.au/arduino/update/"));

	//WiFiClient UpdateClient;
	//t_httpUpdate_return result = ESPhttpUpdate.update(UpdateClient, F("http://10.1.1.93/arduino/update/"));

	switch(result) {
		case HTTP_UPDATE_FAILED:
			update_status += F(" - Update failed: ");
			update_status += ESPhttpUpdate.getLastError();
			update_status += F(" - Error: ");
			update_status += ESPhttpUpdate.getLastErrorString().c_str();
			update_status += F("\n");
			break;
		case HTTP_UPDATE_NO_UPDATES:
			update_status += F(" - No Update Available.\n");
			break;
		case HTTP_UPDATE_OK:
			update_status += F(" - Updated OK.\n");
			break;
	}

	return update_status;
}
#endif
