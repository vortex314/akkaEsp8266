#include "System.h"

#include <FreeRTOS.h>
#include <task.h>

void logHeap() {
	/*  INFO(" heap:%d max_block:%d stack:%d ", heap_caps_get_free_size(MALLOC_CAP_32BIT),
	      heap_caps_get_largest_free_block(MALLOC_CAP_32BIT), uxTaskGetStackHighWaterMark(NULL));*/
	INFO("  free heap : %d stack:%d ", xPortGetFreeHeapSize(), uxTaskGetStackHighWaterMark(NULL));
}

System::System(va_list args)
	: _led(DigitalOut::create(13))
	, _relais(DigitalOut::create(12))
	, _led1(DigitalOut::create(16))
	, _led2(DigitalOut::create(2)) {
	_mqtt = va_arg(args,ActorRef);
}

System::~System() {}

void System::preStart() {
	_led.init();
	_led1.init();
	_led2.init();
	_relais.init();
	_reportTimer = timers().startPeriodicTimer("REPORT_TIMER", TimerExpired(), 5000);
	_ledTimer = timers().startPeriodicTimer("LED_TIMER", TimerExpired(), 100);
	_relaisTimer = timers().startPeriodicTimer("RELAIS_TIMER", TimerExpired(), 100000);
	eb.subscribe(self(), MessageClassifier(_mqtt,Mqtt::Disconnected));
	eb.subscribe(self(), MessageClassifier(_mqtt,Mqtt::Connected));
}

Receive& System::createReceive() {
	return receiveBuilder()
	.match(ReceiveTimeout(), [this](Envelope& msg) { INFO(" No more messages since some time "); })

	.match(TimerExpired(),
	[this](Envelope& msg) {
		uint32_t k;
		msg.get(AKKA_TIMER, k);
		if(Uid(k) == _ledTimer) {
/*			static uint64_t lastTime;
			INFO(" delta Led : %u ",Sys::millis()-lastTime);
			lastTime=Sys::millis();*/
			static bool ledOn = false;
			_led.write(ledOn ? 1 : 0);
			_led1.write(ledOn ? 1 : 0);
			_led2.write(ledOn ? 1 : 0);

			ledOn = !ledOn;
		} else if(Uid(k) == _reportTimer) {
			logHeap();
			_mqtt.tell(Msg(Mqtt::Publish)("topic","src/tester/publish")("message","data"),self());
		} else if(Uid(k) == _relaisTimer) {
			INFO("relais");
			static bool relaisOn = false;
			_relais.write(relaisOn ? 1 : 0);
			relaisOn = !relaisOn;
		}
	})

	.match(
	    Mqtt::Connected,
	[this](Envelope& msg) { INFO(" MQTT Connected "); timers().find(_ledTimer)->interval(500); })

	.match(
	    Mqtt::Disconnected,
	[this](Envelope& msg) { INFO(" MQTT Disconnected "); timers().find(_ledTimer)->interval(100); })

	.match(Properties(),[this](Envelope& msg) {
		INFO(" Properties requested ");

		Msg m(PropertiesReply());
		m("cpu","ESP8266");
		m("procs",1);
		m("upTime",Sys::millis());
		m("ram",80*1024);
		m("heap",xPortGetFreeHeapSize());
		sender().tell(m,self());
	})
	.build();
}
