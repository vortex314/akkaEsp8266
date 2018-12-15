#include "Mqtt.h"

//#define MQTT_PORT 1883
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
MsgClass Mqtt::Connected("Connected");
MsgClass Mqtt::Disconnected("Disconnected");
MsgClass Mqtt::MqttPublishRcvd("MqttPublishRcvd");

void Mqtt::mqttYieldTask(void* pvParameter)
{
    while(true) {
	//	INFO(" heap:%d  stack:%d ", xPortGetFreeHeapSize(), uxTaskGetStackHighWaterMark(NULL));
	if(_mqtt->_mqttConnected) {
	    mqtt_yield(&_mqtt->_client, 1000);
	} else {
	    vTaskDelay(1000);
	}
    }
}

void Mqtt::topic_received_cb(mqtt_message_data_t* md)
{
    mqtt_message_t* message = md->message;

    std::string topic((char*)md->topic->lenstring.data, (uint32_t)md->topic->lenstring.len);
    std::string dta((char*)(message->payload), (uint32_t)message->payloadlen);
    _mqtt->self().tell(_mqtt->self(), MqttPublishRcvd, "++", &topic, &dta);
}

Mqtt::Mqtt(va_list args)
{
    _wifiConnected = false;
    _mqttConnected = false;
    Uid::hash("topic");
    Uid::hash("message");
    _mqtt = this;
}

static bool alive = true;

void Mqtt::preStart()
{
    _topicAlive = "src/";
    _topicAlive += context().system().label();
    _topicAlive += "/system/alive";
    _topicsForDevice = "dst/";
    _topicsForDevice += context().system().label();
    _topicsForDevice += "/#";
    _mqttConnected = false;
    context().setReceiveTimeout(5000);
    _timerAlive = timers().startPeriodicTimer("ALIVE_TIMER", TimerExpired, 5000);
    _timerYield = timers().startPeriodicTimer("YIELD_TIMER", TimerExpired, 10);
    xTaskCreate(&mqttYieldTask, "mqttYieldTask", 500, NULL, tskIDLE_PRIORITY + 1, NULL);
}

Receive& Mqtt::createReceive()
{
    return receiveBuilder()
        .match(AnyClass,
            [this](Envelope& msg) {
	        if(!(*msg.receiver == self())) {
	            INFO(" message received %s:%s:%s [%d] ", msg.sender->path(), msg.receiver->path(),
	                msg.msgClass.label(), msg.message.length());
	            _jsonBuffer.clear();
	            std::string s;
	            JsonArray& array = _jsonBuffer.createArray();
	            s = context().system().label();
	            s += "/";
	            s += msg.sender->path();
	            array.add(msg.receiver->path());
	            array.add(s.c_str());
	            array.add(msg.msgClass.label());
	            array.add(msg.id);
	            payloadToJsonArray(array, msg.message);

	            std::string topic = "dst/";
	            topic += msg.receiver->path();

	            std::string message;
	            array.printTo(message);

	            mqttPublish(topic.c_str(), message.c_str());
	        }
            })
        .match(MqttPublishRcvd,
            [this](Envelope& msg) {
	        string topic;
	        string message;

	        if(msg.scanf("++", &topic, &message) && handleMqttMessage(message.c_str())) {
	            INFO(" processed message %s", message.c_str());
	        } else {
	            WARN(" processing failed : %s ", message.c_str());
	        }
            })
        .match(TimerExpired,
            [this](Envelope& msg) {
	        uint16_t k;
	        msg.scanf("i", &k);
	        if(Uid(k) == _timerYield) {
	            if(_mqttConnected) {
		        //	        int ret = mqtt_yield(&_client, 10);
		        //	        if(ret == MQTT_DISCONNECTED) { _mqttConnected = false; }
	            } else if(_wifiConnected) {
		        mqttConnect();
	            }
	        } else if(Uid(k) == _timerAlive && _mqttConnected) {
	            mqttPublish(_topicAlive.c_str(), "true");
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

void Mqtt::mqttPublish(const char* topic, const char* msg)
{
    if(_mqttConnected) {
	INFO(" MQTT TXD : %s = %s", topic, msg);
	mqtt_message_t message;
	message.payload = (void*)msg;
	message.payloadlen = strlen(msg);
	message.dup = 0;
	message.qos = MQTT_QOS0;
	message.retained = 0;
	_ret = mqtt_publish(&_client, topic, &message);
	if(_ret != MQTT_SUCCESS) { INFO("error while publishing message: %d", _ret); }
    }
}

void Mqtt::mqttSubscribe(const char* topic)
{
    INFO("Subscribing to topic %s ", topic);
    mqtt_subscribe(&_client, topic, MQTT_QOS1, topic_received_cb);
}

bool Mqtt::mqttConnect()
{
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
    mqtt_client_new(&_client, &_network, 5000, _mqtt_buf, 100, _mqtt_readbuf, 100);

    _data.willFlag = 0;
    _data.MQTTVersion = 3;
    _data.clientID.cstring = _mqtt_client_id;
    _data.username.cstring = (char*)MQTT_USER;
    //   _data.username.lenstring=0;
    _data.password.cstring = (char*)MQTT_PASS;
    //    _data.password.lenstring=0;
    _data.keepAliveInterval = 20;
    _data.cleansession = 0;
    _data.will.topicName.cstring = (char*)_topicAlive.data();
    _data.will.message.cstring = (char*)"false";
    INFO("Send MQTT connect ... ");
    _ret = mqtt_connect(&_client, &_data);
    if(_ret) {
	INFO("error: %d", _ret);
	mqtt_network_disconnect(&_network);
	return false;
    };
    mqttSubscribe(_topicsForDevice.c_str());
    Envelope env(self(), Mqtt::Connected);
    eb.publish(env);
    _mqttConnected = true;
    return true;
}

void Mqtt::mqttDisconnect() { mqtt_network_disconnect(&_network); }

bool Mqtt::payloadToJsonArray(JsonArray& array, Cbor& payload)
{
    Cbor::PackType pt;
    payload.offset(0);
    Erc rc;
    string str;
    while(payload.hasData()) {
	rc = payload.peekToken(pt);
	if(rc != E_OK) break;
	switch(pt) {
	case Cbor::P_STRING: {
	    payload.get(str);
	    array.add((char*)str.c_str());
	    // const char* is not copied into buffer
	    break;
	}
	case Cbor::P_PINT: {
	    uint64_t u64;
	    payload.get(u64);
	    array.add(u64);
	    break;
	}
	case Cbor::P_DOUBLE: {
	    double d;
	    payload.get(d);
	    array.add(d);
	    break;
	}
	default: {
	    payload.skipToken();
	}
	}
    };
    return true;
}

bool Mqtt::handleMqttMessage(const char* message)
{
    //    Envelope envelope(1024);
    _jsonBuffer.clear();
    JsonArray& array = _jsonBuffer.parse(message);
    if(array == JsonArray::invalid()) return false;
    if(array.size() < 4) return false;
    if(!array.is<char*>(0) || !array.is<char*>(1) || !array.is<char*>(2)) return false;

    string rcvs = array.get<const char*>(0);

    ActorRef* rcv = ActorRef::lookup(Uid(rcvs.c_str()));
    if(rcv == 0) {
	rcv = &context().system().actorFor(rcvs.c_str());
	ASSERT(rcv != 0);
	WARN(" local Actor : %s not found ", rcvs.c_str());
    }
    ActorRef* snd = ActorRef::lookup(Uid::hash(array.get<const char*>(1)));
    if(snd == 0) {
	snd = &context().system().actorFor(array.get<const char*>(1));
	ASSERT(snd != 0);
    }
    MsgClass cls(array.get<const char*>(2));
    uint16_t id = array.get<int>(3);

    for(uint32_t i = 4; i < array.size(); i++) {
	// TODO add to cbor buffer
	if(array.is<char*>(i)) {
	    txdEnvelope().message.add(array.get<const char*>(i));
	} else if(array.is<int>(i)) {
	    txdEnvelope().message.add(array.get<int>(i));
	} else if(array.is<double>(i)) {
	    txdEnvelope().message.add(array.get<double>(i));
	} else if(array.is<bool>(i)) {
	    txdEnvelope().message.add(array.get<bool>(i));
	} else {
	    WARN(" unhandled data type in MQTT message ");
	}
    }
    txdEnvelope().header(*rcv, *snd, cls, id);
    rcv->tell(*snd, txdEnvelope());
    return true;
}