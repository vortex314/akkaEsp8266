#include "Mqtt.h"

//#define MQTT_PORT 1883
// .match(Exit, [](Msg& msg) { exit(0); })
#define MQTT_HOST "limero.ddns.net"
#define MQTT_PORT 1883
#define MQTT_USER ""

#define MQTT_PASS ""

enum {
	WIFI_CONNECTED = 0,    // 0
	WIFI_DISCONNECTED,     // 1
	MQTT_CONNECTED,        // 2
	SIG_MQTT_DISCONNECTED, // 3
	MQTT_SUBSCRIBED,       // 4
	MQTT_PUBLISHED,        // 5
	MQTT_INCOMING,         // 6
	SIG_MQTT_FAILURE,      // 7
	MQTT_DO_PUBLISH        // 8
};

Mqtt* Mqtt::_mqtt = 0;
MsgClass Mqtt::Connected("connected");
MsgClass Mqtt::Disconnected("disconnected");
MsgClass Mqtt::PublishRcvd("publishRcvd");
MsgClass Mqtt::Publish("publish");
MsgClass Mqtt::Subscribe("subscribe");

Mqtt::Mqtt(ActorRef& wifi)
		: _wifi(wifi) {
//	_address = va_arg(args, const char*);
	_address = MQTT_HOST;
	_wifiConnected = false;
	_mqttConnected = false;
	_mqtt = this;
}

void Mqtt::preStart() {
	_topicAlive = "src/";
	_topicAlive += context().system().label();
	_topicAlive += "/system/alive";

	_topicsForDevice = "dst/";
	_topicsForDevice += context().system().label();
	_topicsForDevice += "/#";

	_mqttConnected = false;
	_wifiConnected = false;

	_timerYield =
			timers().startPeriodicTimer("YIELD_TIMER", Msg("yieldTimer"), 5000);

	eb.subscribe(self(), MessageClassifier(_wifi, Wifi::Disconnected));
	eb.subscribe(self(), MessageClassifier(_wifi, Wifi::Connected));
}

Receive& Mqtt::createReceive() {
	return receiveBuilder()

	.match(MsgClass::ReceiveTimeout(), [this](Msg& msg) {
		INFO("ReceiveTimeout");
	})

	.match(Mqtt::Publish, [this](Msg& msg) {
		std::string topic;
		std::string message;
		if ( msg.get("topic",topic)==0 && msg.get("message",message)==0 ) {
			mqttPublish(topic,message);
		}
	})

	.match(MsgClass("yieldTimer"), [this](Msg& msg) {
		if(_mqttConnected) {
			int ret = mqtt_yield(&_client, 0);
			if(ret == MQTT_DISCONNECTED) {
				mqttDisconnect();
				mqttConnect();
			}
		} else if(_wifiConnected) {
			mqttConnect();
		}
	})

	.match(Wifi::Connected, [this](Msg& msg) {
		_wifiConnected=true;
		mqttConnect();
	})

	.match(Wifi::Disconnected, [this](Msg& msg) {
		_wifiConnected = false;
		if ( _mqttConnected )	mqttDisconnect();
	})

	.match(MsgClass::Properties(), [this](Msg& msg) {
		sender().tell(replyBuilder(msg)
				("host",_address)
				("port",MQTT_PORT)
				("stack",(uint32_t)uxTaskGetStackHighWaterMark(NULL))
				,self());
	}).build();
}
/*
 void Mqtt::mqttYieldTask(void* pvParameter) {
 while(true) {
 //	INFO(" heap:%d  stack:%d ", xPortGetFreeHeapSize(), uxTaskGetStackHighWaterMark(NULL));
 if(_mqtt->_mqttConnected) {
 mqtt_yield(&_mqtt->_client, 1000);
 } else {
 vTaskDelay(1000);
 }
 }
 }
 */
void Mqtt::topic_received_cb(mqtt_message_data_t* md) {
	static bool busy = false;
	if (!busy) {
		mqtt_message_t* message = md->message;
		std::string topic((char*) md->topic->lenstring.data, (uint32_t) md
				->topic->lenstring.len);
		std::string data((char*) (message->payload), (uint32_t) message
				->payloadlen);
		busy = true;
		INFO("MQTT_EVENT_DATA");
		//   me->self().tell(me-self(),MQTT_PUBLISH_RCVD,"SS",&topic,&msg);
		Msg pub(Mqtt::PublishRcvd);
		pub("topic", topic);
		pub("message", data);
		pub.src(_mqtt->self().id());
//		INFO("%s",pub.toString().c_str());
		eb.publish(pub);
		busy = false;
	} else {
		WARN(" sorry ! MQTT reception busy ");
	}
}

void Mqtt::mqttPublish(std::string& topic, std::string& msg) {
	if (_mqttConnected) {
		uint64_t start=Sys::millis();
		DEBUG(" MQTT TXD : %s = %s", topic.c_str(), msg.c_str());
		mqtt_message_t message;
		message.payload = (void*) msg.data();
		message.payloadlen = msg.size();
		message.dup = 0;
		message.qos = MQTT_QOS0;
		message.retained = 0;
		_ret = mqtt_publish(&_client, topic.c_str(), &message);
		if (_ret != MQTT_SUCCESS) {
			INFO("error while publishing message: %d", _ret);
			mqttDisconnect();
			return;
		}
		_ret = mqtt_yield(&_client,0);
		if (_ret == MQTT_DISCONNECTED ) {
			WARN(" mqtt_yield failed : %d ", _ret);
			mqttDisconnect();
		}
//		INFO(" delay MQTT publish %s %d ",topic.c_str(),(uint32_t)(Sys::millis()-start));
	} else {
//		WARN(" cannot publish : disconnected. ");
	}
}

void Mqtt::mqttSubscribe(const char* topic) {
	INFO("Subscribing to topic %s ", topic);
	mqtt_subscribe(&_client, topic, MQTT_QOS1, topic_received_cb);
}

bool Mqtt::mqttConnect() {
	_mqttConnected = false;
	_ret = 0;
	_client = mqtt_client_default;
	_data = mqtt_packet_connect_data_initializer;
	mqtt_network_new(&_network);
	ZERO(_mqtt_client_id);
	strcpy(_mqtt_client_id, Sys::hostname());

	INFO("connecting to MQTT server %s:%d ... ", MQTT_HOST, MQTT_PORT);
	_ret = mqtt_network_connect(&_network, MQTT_HOST, MQTT_PORT);
	if (_ret) {
		INFO("matt connect error: %d", _ret);
		return false;
	}
	mqtt_client_new(&_client, &_network, 3000, _mqtt_buf, 1024, _mqtt_readbuf, 1024);

	_data.willFlag = 0;
	_data.MQTTVersion = 3;
	_data.clientID.cstring = _mqtt_client_id;
	_data.username.cstring = (char*) MQTT_USER;
	//   _data.username.lenstring=0;
	_data.password.cstring = (char*) MQTT_PASS;
	//    _data.password.lenstring=0;
	_data.keepAliveInterval = 20;
	_data.cleansession = 0;
	_data.will.topicName.cstring = (char*) _topicAlive.c_str();
	_data.will.message.cstring = (char*) "false";
	_ret = mqtt_connect(&_client, &_data);
	if (_ret) {
		eb.publish(Msg(Mqtt::Disconnected).src(self().id()));
		INFO("mqtt connect error: %d", _ret);
		mqtt_network_disconnect(&_network);
		return false;
	};
	mqttSubscribe(_topicsForDevice.c_str());
	eb.publish(Msg(Mqtt::Connected).src(self().id()));
	_mqttConnected = true;
	INFO("mqtt connect success.");
	return true;
}

void Mqtt::mqttDisconnect() {
	INFO(" disconnecting.");
	mqtt_disconnect(&_client);
	mqtt_network_disconnect(&_network);
	_mqttConnected = false;
	eb.publish(Msg(Disconnected).src(self().id()));
}
