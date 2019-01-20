#ifndef MQTT_H
#define MQTT_H
#include "lwip/err.h"
#include "lwip/ip_addr.h"
#include "lwip/prot/iana.h"

#include "esp/uart.h"
#include "espressif/esp_common.h"

#include <string.h>

#include <FreeRTOS.h>
#include <task.h>

#include <espressif/esp_sta.h>
#include <espressif/esp_wifi.h>

#include <Akka.h>
#include <Wifi.h>

#include <ArduinoJson.h>

extern "C" {
#include <paho_mqtt_c/MQTTClient.h>
#include <paho_mqtt_c/MQTTESP8266.h>
}

class Mqtt : public Actor {
		int _ret = 0;
		struct mqtt_network _network;
		mqtt_client_t _client = mqtt_client_default;
		char _mqtt_client_id[20];
		uint8_t _mqtt_buf[512];
		uint8_t _mqtt_readbuf[512];
		mqtt_packet_connect_data_t _data;
		std::string _topicAlive;
		std::string _topicsForDevice;
		bool _wifiConnected;
		bool _mqttConnected;
//		bool _receiving;
		StaticJsonBuffer<2000> _jsonBuffer;
		std::string _clientId;
		std::string _address;
		Label _timerYield;
		Label _timerAlive;
		ActorRef& _wifi;

	public:
		static Mqtt* _mqtt;
		static MsgClass Connected;
		static MsgClass Disconnected;
		static MsgClass PublishRcvd;
		static MsgClass Publish;
		static MsgClass Subscribe;


		Mqtt(ActorRef& wifi);
		virtual ~Mqtt() {};

		void publish(std::string& topic, std::string& message);
		static void topic_received_cb(mqtt_message_data_t* md);
		void onMessageReceived(std::string& topic, std::string& payload);

		void mqttPublish(std::string& topic, std::string& message);
		void mqttSubscribe(const char* topic);
		bool mqttConnect();
		void mqttDisconnect();
		bool handleMqttMessage(const char* message);

		void preStart();
		Receive& createReceive();
//		static bool payloadToJsonArray(JsonArray& array, Cbor& payload);
		static void mqttYieldTask(void* pvParameter);

	private:
		void mqtt_do_connect();
};

#endif // MQTT_H
