
#include <Wifi.h>
MsgClass Wifi::Connected("Wifi/Connected");
MsgClass Wifi::Disconnected("Wifi/Disconnected");

Wifi* Wifi::_wifi = 0; // system sigleton, needed in callback

Wifi::Wifi(va_list args) {
	_wifi = this;
	_foundAP = false;
	_ssidPattern = "Merckx";
	_pswd = PSWD;
	_state = START_SCAN;
};

void Wifi::preStart() {
	timers().startPeriodicTimer("INTERVAL", Msg("check"), 2000);
	ZERO(_config);
	strcpy((char*)_config.password, _pswd.c_str());
	if(sdk_wifi_get_opmode() == SOFTAP_MODE) { INFO("ap mode can't scan !!!\r\n"); }
	_foundAP = false;

	SCAN = &receiveBuilder()
	       .match(MsgClass("check"),
	[this](Envelope&) {
		if(_state == START_SCAN) {
			_foundAP=false;
			_state = SCANNING;
			_retries=10;
			startScan();
		} else if(_state == SCANNING) {
			if(_foundAP) {
				INFO(" Scan end, ssid : %s ",_ssid.c_str());
				strcpy((char*)_config.ssid, _ssid.c_str());
				sdk_wifi_station_disconnect();
				_state = START_CONNECT;
				context().become(*DISCONNECTED, true);
				return;
			} else {
				INFO("Wait for scan end... %d ",_retries--);
				if ( _retries==0) {
					_state=START_SCAN;
					INFO(" restarting scan ");
				}
			}
		} else {
			WARN(" unknown state %d",_state);
		}
	})
	.build();

	// https://github.com/SuperHouse/esp-open-rtos/issues/333

	DISCONNECTED = &receiveBuilder()
	               .match(MsgClass("check"),
	[this](Envelope&) {
		if ( _state==START_CONNECT ) {
			sdk_wifi_station_disconnect();
			netif_set_hostname(netif_default, Sys::hostname());
			sdk_wifi_set_opmode(STATION_MODE);
			sdk_wifi_station_set_config(&_config);
			sdk_wifi_station_connect();
			_retries=10;
			_state = CONNECTING;
		} else if ( _state==CONNECTING ) {
			_status = sdk_wifi_station_get_connect_status();
			INFO("Wifi status =%d , retries left: %d", _status, _retries);
			if(_status == STATION_WRONG_PASSWORD) {
				INFO("WiFi: wrong password");
			} else if(_status == STATION_NO_AP_FOUND) {
				INFO("WiFi: AP not found");
			} else if(_status == STATION_CONNECT_FAIL) {
				INFO("WiFi: connection failed");
			} else if ( _status == STATION_GOT_IP ) {
				becomeConnected();
			} else if ( _status == STATION_IDLE ) {
				INFO("WiFi: STATION_IDLE");
			} else if ( _status == STATION_CONNECTING ) {
				INFO("WiFi: STATION_CONNECTING");
			} else {
				WARN(" unknown wifi state: %d",_status);
			}
			--_retries;
			if(_retries == 0) {
				_foundAP = false;
				_state=START_SCAN;
				context().become(*SCAN);
			}
		}
	})
	.build();

	CONNECTED = &receiveBuilder()
	            .match(MsgClass("check"),
	[this](Envelope&) {
		if(!isConnected()) {
			INFO("disconnected...");
			Msg  m(Wifi::Disconnected);
			m.src(self().id());
			eb.publish(m);
			sdk_wifi_station_disconnect();
			_foundAP = false;
			_state=START_SCAN;
			context().become(*SCAN);
		} else {
			//INFO("connected...");
		}
	})
	.match(Properties(),[this](Envelope& msg) {
//		INFO("%s",msg.toString().c_str());

		uint8_t mac[13];
		sdk_wifi_get_macaddr(STATION_IF, (uint8_t *) mac);
		std::string macAddress;
		string_format(macAddress,"%02X:%02X:%02X:%02X:%02X:%02X",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
		sender().tell(msg.reply()
		              ("ip",_ipAddress)
		              ("mac",macAddress)
		              ("ssid",_ssid)
		              ("rssi",_rssi)
		              ,self());
	})
	.build();
	_state = START_SCAN;
	context().become(*SCAN);
}

Receive& Wifi::createReceive() { return *SCAN; }

static const char* const auth_modes[] = { [AUTH_OPEN] = "Open",
                                          [AUTH_WEP] = "WEP",
                                          [AUTH_WPA_PSK] = "WPA/PSK",
                                          [AUTH_WPA2_PSK] = "WPA2/PSK",
                                          [AUTH_WPA_WPA2_PSK] = "WPA/WPA2/PSK"
                                        };

void Wifi::scan_done_cb(void* arg, sdk_scan_status_t status) {
	char ssid[33]; // max SSID length + zero byte
	static string ssidStr;

	INFO("scan_done_cb()");

	if(status != SCAN_OK) {
		WARN("Error: WiFi scan failed\n");
		return;
	}

	struct sdk_bss_info* bss = (struct sdk_bss_info*)arg;
	// first one is invalid
	bss = bss->next.stqe_next;

	struct sdk_bss_info* strongestAP = 0;
	int strongestRssi = -1000;
	while(NULL != bss) {
		size_t len = strlen((const char*)bss->ssid);
		memcpy(ssid, bss->ssid, len);
		ssid[len] = 0;

		ssidStr = ssid;

		INFO("%32s (" MACSTR ") RSSI: %02d, security: %s", ssid, MAC2STR(bss->bssid), bss->rssi,
		     auth_modes[bss->authmode]);
		if(ssidStr.find(_wifi->_ssidPattern) == 0 && bss->rssi > strongestRssi) {
			strongestAP = bss;
			strongestRssi = bss->rssi;
		}
		bss = bss->next.stqe_next;
	}
	if(strongestAP) {
		_wifi->_ssid = (const char*)strongestAP->ssid;
		_wifi->_rssi = strongestAP->rssi;
		_wifi->_foundAP = true;
		INFO(" selected AP : %s ", _wifi->_ssid.c_str());
		return;
	}
}

void Wifi::startScan() {
	INFO("Start scanning.. %s ", Sys::hostname());
	int erc;
	erc = sdk_wifi_set_opmode(STATION_MODE);
	INFO("sdk_wifi_set_opmode(STATION_MODE) = %d", erc);
	erc = sdk_wifi_station_scan(NULL, scan_done_cb);
	INFO("sdk_wifi_station_scan(NULL, scan_done_cb) = %d", erc);
	eb.publish(Msg(Wifi::Disconnected).src(self().id()));
}

bool Wifi::isConnected() {
	_status = sdk_wifi_station_get_connect_status();
	_connected = (_status == STATION_GOT_IP);
	return _connected;
}

void Wifi::becomeConnected() {
	ip_info ipInfo;
	if(sdk_wifi_get_ip_info(STATION_IF, &ipInfo)) {
		_ipAddress = ip4addr_ntoa((ip4_addr_t*)&ipInfo.ip.addr);
		INFO(" ip info %s", _ipAddress.c_str());
	}
	INFO("WiFi: Connected");
	Msg  m(Wifi::Connected);
	m.src(self().id());
	eb.publish(m);
	context().become(*CONNECTED);
}
