#ifndef _WIFI_H_
#define _WIFI_H_

#include "espressif/esp_common.h"
#include <FreeRTOS.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_wifi.h>
#include <task.h>

#include <Akka.h>

class Wifi : public Actor
{
    uint8_t status = 0;
    struct sdk_station_config _config;
    string _ssidPattern;
    string _ssid;
    string _pswd;
    string _ipAddress;
    bool _foundAP;
    static Wifi* _wifi;
    int _rssi;

    Receive* SCAN;
    Receive* DISCONNECTED;
    Receive* CONNECTED;
    uint32_t _retries;
    uint8_t _status;
    bool _connected;
    enum { START, SCANNING, CONNECTING, ONLINE } _state;

public:
    static MsgClass Connected;
    static MsgClass Disconnected;
    Wifi(va_list args);

    void preStart();
    Receive& createReceive();
    static void scan_done_cb(void* arg, sdk_scan_status_t status);
    void scan();
    bool isConnected();
    void becomeConnected();
};
#endif
