#ifndef _WIFI_H_
#define _WIFI_H_

#include "espressif/esp_common.h"
#include <FreeRTOS.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_system.h>
#include <lwip/api.h>
#include <lwip/netif.h>
#include <task.h>

#include <Akka.h>

class Wifi : public Actor {
		uint8_t status = 0;
		struct sdk_station_config _config;
		std::string _ssidPattern;
		std::string _ssid;
		std::string _pswd;
		std::string _ipAddress;
		bool _foundAP;
		static Wifi* _wifi;
		int _rssi;

		Receive* SCAN;
		Receive* DISCONNECTED;
		Receive* CONNECTED;
		uint32_t _retries;
		uint8_t _status;
		bool _connected;
		enum { START_SCAN, SCANNING, START_CONNECT,CONNECTING, ONLINE } _state;

	public:
		static MsgClass Connected;
		static MsgClass Disconnected;
		Wifi();

		void preStart();
		Receive& createReceive();
		static void scan_done_cb(void* arg, sdk_scan_status_t status);
		void scan();
		void startScan();
		bool isConnected();
		void becomeConnected();
};
#endif
