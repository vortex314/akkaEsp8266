PATH  := /home/lieven/workspace/esp-open-sdk/xtensa-lx106-elf/bin:$(PATH)
SHELL := env PATH=$(PATH) /bin/bash
PROGRAM=akkaEsp8266
EXTRA_COMPONENTS = extras/sntp extras/mdnsresponder extras/rboot-ota ../Common  ../ArduinoJson  extras/paho_mqtt_c
# EXTRA_COMPONENTS = extras/paho_mqtt_c ../Common ../Ebos ../ArduinoJson
PROGRAM_SRC_DIR=. main
PROGRAM_INC_DIR=. main ../microAkka/src ../esp-open-rtos/include ../esp-open-rtos/core/include ../esp-open-sdk/lx106-hal/include ../Common  ../ArduinoJson ../DWM1000 ../etl/src  $(ROOT)bootloader $(ROOT)bootloader/rboot
PROGRAM_CXXFLAGS += -ffunction-sections -fdata-sections  -fno-threadsafe-statics -std=c++11 -fno-rtti -lstdc++ -fno-exceptions -DPSWD=${PSWD} -DSSID=${SSID} -DESP_OPEN_RTOS
# PROGRAM_INC_DIR=. ../esp-open-rtos/include ../Common ../Ebos ../ArduinoJson ../esp-open-rtos/lwip/lwip/src/include 
ESPBAUD=921600
TTY ?= USB0
SERIAL_PORT ?= /dev/tty$(TTY)
ESPPORT = $(SERIAL_PORT)
SERIAL_BAUD = 115200
LIBS= m hal gcc stdc++
EXTRA_CFLAGS=-DEXTRAS_MDNS_RESPONDER -DLWIP_MDNS_RESPONDER=1 -DLWIP_NUM_NETIF_CLIENT_DATA=1 -DLWIP_NETIF_EXT_STATUS_CALLBACK=1 -DPSWD=${PSWD} -DSSID=${SSID}
EXTRA_LDFLAGS= -Wl,--gc-sections
EXTRA_CXXFLAGS += -O3 -ffunction-sections -fdata-sections  -fno-threadsafe-statics -std=c++11 -fno-rtti -lstdc++ -fno-exceptions -DPSWD=${PSWD} -DSSID=${SSID} -DESP_OPEN_RTOS

# FLAVOR=sdklike
# -Wl,--gc-sections
include ../esp-open-rtos/common.mk

term:
	rm -f $(TTY)_minicom.log
	minicom -D $(SERIAL_PORT) -b $(SERIAL_BAUD) -C $(TTY)_minicom.log
