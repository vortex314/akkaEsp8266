
#include <Wifi.h>

MsgClass Wifi::Connected("Connected");
MsgClass Wifi::Disconnected("Disconnected");

Wifi* Wifi::_wifi = 0; // system sigleton, needed in callback

Wifi::Wifi(va_list args)
{
    _wifi = this;
    _foundAP = false;
    _ssidPattern = "Merckx";
    _pswd = PSWD;
    _state = START;
};

#include <lwip/api.h>
#include <lwip/netif.h>

void Wifi::preStart()
{
    context().setReceiveTimeout(3000);
    ZERO(_config);
    strcpy((char*)_config.password, _pswd.c_str());
    _foundAP = false;
    if(sdk_wifi_get_opmode() == SOFTAP_MODE) { INFO("ap mode can't scan !!!\r\n"); }

    SCAN = &receiveBuilder()
                .match(AnyClass,
                    [this](Envelope&) {
	                if(isConnected()) {
	                    becomeConnected();
	                    return;
	                }

	                if(_state == START) {
	                    INFO("Start scanning.. %s ", Sys::hostname());
	                    int erc;
	                    //	                    erc = sdk_wifi_station_disconnect();
	                    //	                    INFO("sdk_wifi_station_disconnect() = %d", erc);
	                    //	                    ASSERT(netif_default);
	                    //	                    netif_set_hostname(netif_default, Sys::hostname());
	                    erc = sdk_wifi_set_opmode(STATION_MODE);
	                    INFO("sdk_wifi_set_opmode(STATION_MODE) = %d", erc);
	                    erc = sdk_wifi_station_scan(NULL, scan_done_cb);
	                    INFO("sdk_wifi_station_scan(NULL, scan_done_cb) = %d", erc);
	                    _state = SCANNING;
	                } else if(_state == SCANNING) {
	                    INFO("Scanning...");
	                    if(_foundAP) {
		                strcpy((char*)_config.ssid, _ssid.c_str());
		                _retries = 10;
		                sdk_wifi_station_disconnect();
		                _state = CONNECTING;
		                context().become(*DISCONNECTED, true);
		                return;
	                    } else {
		                sdk_wifi_station_scan(NULL, scan_done_cb);
	                    }
	                }
                    })
                .build();

    // https://github.com/SuperHouse/esp-open-rtos/issues/333

    DISCONNECTED = &receiveBuilder()
                        .match(AnyClass,
                            [this](Envelope&) {
	                        INFO("CONNECTING...");
	                        if(isConnected()) {
	                            becomeConnected();
	                            return;
	                        }

	                        sdk_wifi_station_disconnect();
	                        netif_set_hostname(netif_default, Sys::hostname());
	                        sdk_wifi_set_opmode(STATION_MODE);
	                        sdk_wifi_station_set_config(&_config);
	                        sdk_wifi_station_connect();

	                        _status = sdk_wifi_station_get_connect_status();
	                        INFO("Wifi status =%d , retries left: %d", _status, _retries);
	                        if(_status == STATION_WRONG_PASSWORD) {
	                            INFO("WiFi: wrong password");
	                        } else if(_status == STATION_NO_AP_FOUND) {
	                            INFO("WiFi: AP not found");
	                        } else if(_status == STATION_CONNECT_FAIL) {
	                            INFO("WiFi: connection failed");
	                        }
	                        --_retries;
	                        if(_retries == 0) {
	                            _foundAP = false;
	                            context().become(*SCAN);
	                        }
                            })
                        .build();
    CONNECTED = &receiveBuilder()
                     .match(AnyClass,
                         [this](Envelope&) {
	                     if(!isConnected()) {
	                         sdk_wifi_station_disconnect();
	                         _foundAP = false;
	                         context().become(*SCAN);
	                     }
                         })
                     .build();
}

Receive& Wifi::createReceive() { return *SCAN; }

static const char* const auth_modes[] = { [AUTH_OPEN] = "Open",
    [AUTH_WEP] = "WEP",
    [AUTH_WPA_PSK] = "WPA/PSK",
    [AUTH_WPA2_PSK] = "WPA2/PSK",
    [AUTH_WPA_WPA2_PSK] = "WPA/WPA2/PSK" };

void Wifi::scan_done_cb(void* arg, sdk_scan_status_t status)
{
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
	_wifi->_state = CONNECTING;
	return;
    } else {
	_wifi->_state = START;
    }
}

bool Wifi::isConnected()
{
    _status = sdk_wifi_station_get_connect_status();
    _connected = (_status == STATION_GOT_IP);
    return _connected;
}

void Wifi::becomeConnected()
{
    ip_info ipInfo;
    if(sdk_wifi_get_ip_info(STATION_IF, &ipInfo)) {
	_ipAddress = ip4addr_ntoa((ip4_addr_t*)&ipInfo.ip.addr);
	INFO(" ip info %s", _ipAddress.c_str());
    }
    INFO("WiFi: Connected");
    Envelope env(self(), Wifi::Connected);
    eb.publish(env);
    context().become(*CONNECTED);
}
