/*
 * Sys.cpp
 *
 *  Created on: May 15, 2016
 *      Author: lieven
 */
#include <FreeRTOS.h>
#include <Log.h>
#include <Sys.h>
#include <espressif/esp_wifi.h>
#include <stdint.h>
#include <sys/time.h>
#include <task.h>

uint64_t Sys::_upTime;

const char* Sys::getProcessor() { return "ESP8266"; }
const char* Sys::getBuild() { return __DATE__ " " __TIME__; }

void Sys::init()
{
    // uint8_t cpuMhz = sdk_system_get_cpu_frequency();
}

uint32_t Sys::getFreeHeap() { return 0; };

char Sys::_hostname[30] = "";
uint64_t Sys::_boot_time = 0;
uint64_t Sys::millis()
{
    /*
    timeval time;
    gettimeofday(&time, NULL);
    return (time.tv_sec * 1000) + (time.tv_usec / 1000);*/
    return Sys::micros() / 1000;
}

uint32_t Sys::sec() { return millis() / 1000; }

uint64_t Sys::micros()
{

    static uint32_t lsClock = 0;
    static uint32_t msClock = 0;

    vPortEnterCritical();
    uint32_t ccount;
    __asm__ __volatile__("esync; rsr %0,ccount" : "=a"(ccount));
    if(ccount < lsClock) { msClock++; }
    lsClock = ccount;
    portEXIT_CRITICAL();

    uint64_t micros = msClock;
    micros <<= 32;
    micros += lsClock;

    return micros / 80;
}

uint64_t Sys::now() { return _boot_time + Sys::millis(); }

void Sys::setNow(uint64_t n) { _boot_time = n - Sys::millis(); }

void Sys::hostname(const char* h) { strncpy(_hostname, h, strlen(h) + 1); }

void Sys::setHostname(const char* h) { strncpy(_hostname, h, strlen(h) + 1); }

void Sys::delay(unsigned int delta)
{
    uint32_t end = Sys::millis() + delta;
    while(Sys::millis() < end)
	;
}

extern "C" uint64_t SysMillis() { return Sys::millis(); }

/*
 *
 *  ATTENTION : LOGF call Sys::hostname, could invoke another call to
 * Sys::hostname with LOGF,....
 *
 *
 *
 */
#if defined(ESP_OPEN_RTOS)
#include <espressif/esp_system.h>
const char* Sys::hostname()
{
    if(_hostname[0] == 0) { snprintf(_hostname, sizeof(_hostname), "ESP82-%d", sdk_system_get_chip_id() & 0xFFFF); }
    return _hostname;
}
const char* Sys::getBoard()
{
    static char buffer[80];
    snprintf(buffer, sizeof(buffer), " cpu-id : %X , freq : %d Mhz rom-sdk : %s", sdk_system_get_chip_id(),
        sdk_system_get_cpu_freq(), sdk_system_get_sdk_version());
    sdk_system_print_meminfo();
    return buffer;
}

#endif

#ifdef __ARDUINO__
#include <Arduino.h>

uint32_t Sys::getFreeHeap() { return ESP.getFreeHeap(); }

uint64_t Sys::millis() { return ::millis(); }
const char* Sys::hostname() { return _hostname; }

#endif
