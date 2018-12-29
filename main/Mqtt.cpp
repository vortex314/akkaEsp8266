#include "Mqtt.h"

//#define MQTT_PORT 1883
// .match(Exit, [](Envelope& msg) { exit(0); })
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
MsgClass Mqtt::Connected("Mqtt/Connected");
MsgClass Mqtt::Disconnected("Mqtt/Disconnected");
MsgClass Mqtt::PublishRcvd("Mqtt/PublishRcvd");
MsgClass Mqtt::Publish("Mqtt/Publish");
MsgClass Mqtt::Subscribe("Mqtt/Subscribe");

Mqtt::Mqtt(va_list args) {
	_wifi = va_arg(args,ActorRef);
	_address = va_arg(args, const char*);
	_wifiConnected = false;
	_mqttConnected = false;
	Uid::hash("topic");
	Uid::hash("message");
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

	_timerAlive = timers().startPeriodicTimer("ALIVE_TIMER", TimerExpired(), 1000);
	_timerYield = timers().startPeriodicTimer("YIELD_TIMER", TimerExpired(), 500);

	eb.subscribe(self(), MessageClassifier(_wifi, Wifi::Disconnected));
	eb.subscribe(self(), MessageClassifier(_wifi, Wifi::Connected));
	//   xTaskCreate(&mqttYieldTask, "mqttYieldTask", 500, NULL, tskIDLE_PRIORITY + 1, NULL);
}

Receive& Mqtt::createReceive() {
	return receiveBuilder()

	.match(ReceiveTimeout(),[this](Envelope& msg) {
		INFO("ReceiveTimeout");
	})

	.match(Mqtt::Publish,
	[this](Envelope& msg) {
//		INFO("%s",msg.toString().c_str());
		std::string topic;
		std::string message;
		if ( msg.get("topic",topic)==0 && msg.get("message",message)==0 ) {
			mqttPublish(topic,message);
		}
	})

	.match(TimerExpired(),
	[this](Envelope& msg) {
		uint16_t k;
		assert(msg.get(UID_TIMER, k)==0);
		Uid key(k);
		if(Uid(k) == _timerYield) {
			if(_mqttConnected) {
				int ret = mqtt_yield(&_client, 10);
				if(ret == MQTT_DISCONNECTED) {
					mqttDisconnect();
					mqttConnect();
				}
			} else if(_wifiConnected) {
				mqttConnect();
			}
		} else if(Uid(k) == _timerAlive ) {
			if ( _mqttConnected) {
				std::string message="true";
				mqttPublish(_topicAlive, message);
			} else {
				mqttDisconnect();
				mqttConnect();
			}
		} else {
			WARN(" unknown timer ,timerId : %d ",k);
			INFO(" msg : %s ",msg.toString().c_str());
		}
	})

	.match(Wifi::Connected,
	[this](Envelope& msg) {
		INFO(" WIFI CONNECTED !!");
		mqttConnect();
	})

	.match(Wifi::Disconnected,
	[this](Envelope& msg) {
		INFO(" WIFI DISCONNECTED !!");
		mqttDisconnect();
	})
	.build();
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
	mqtt_message_t* message = md->message;

	std::string topic((char*)md->topic->lenstring.data, (uint32_t)md->topic->lenstring.len);
	std::string data((char*)(message->payload), (uint32_t)message->payloadlen);
	bool busy=false;
	if ( !busy ) {
		busy=true;
		INFO("MQTT_EVENT_DATA");
		//   me->self().tell(me-self(),MQTT_PUBLISH_RCVD,"SS",&topic,&msg);
		Msg  pub(Mqtt::PublishRcvd);
		pub("topic", topic);
		pub("message", data);
		pub(UID_SRC,_mqtt->self().id());
//		INFO("%s",pub.toString().c_str());
		eb.publish(pub);
		busy=false;
	} else {
		WARN(" sorry ! MQTT reception busy ");
	}
}



static bool alive = true;





void Mqtt::mqttPublish(std::string& topic, std::string& msg) {
	if(_mqttConnected) {
		INFO(" MQTT TXD : %s = %s", topic.c_str(), msg.c_str());
		mqtt_message_t message;
		message.payload = (void*)msg.data();
		message.payloadlen = msg.size();
		message.dup = 0;
		message.qos = MQTT_QOS0;
		message.retained = 0;
		_ret = mqtt_publish(&_client, topic.c_str(), &message);
		if(_ret != MQTT_SUCCESS) { INFO("error while publishing message: %d", _ret); mqttDisconnect(); }
	} else {
		WARN(" cannot publish : disconnected. ");
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
	if(_ret) {
		INFO("connect error: %d", _ret);
		return false;
	}
	mqtt_client_new(&_client, &_network, 5000, _mqtt_buf, 256, _mqtt_readbuf, 256);

	_data.willFlag = 0;
	_data.MQTTVersion = 3;
	_data.clientID.cstring = _mqtt_client_id;
	_data.username.cstring = (char*)MQTT_USER;
	//   _data.username.lenstring=0;
	_data.password.cstring = (char*)MQTT_PASS;
	//    _data.password.lenstring=0;
	_data.keepAliveInterval = 20;
	_data.cleansession = 0;
	_data.will.topicName.cstring = (char*)_topicAlive.c_str();
	_data.will.message.cstring = (char*)"false";
	INFO("Send MQTT connect ... ");
	_ret = mqtt_connect(&_client, &_data);
	if(_ret) {
		Msg msg(Disconnected);
		msg(UID_SRC,_mqtt->self().id());
		eb.publish(msg);
		INFO("error: %d", _ret);
		mqtt_network_disconnect(&_network);
		return false;
	};
	mqttSubscribe(_topicsForDevice.c_str());
	Msg msg(Connected);
	msg(UID_SRC,_mqtt->self().id());
	eb.publish(msg);
	_mqttConnected=true;
	return true;
}

void Mqtt::mqttDisconnect() {
	INFO(" disconencting.");
	mqtt_network_disconnect(&_network);
	_mqttConnected=false;
	eb.publish(Msg(Disconnected).src(self()));
	mqttConnect();
}
