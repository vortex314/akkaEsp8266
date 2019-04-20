#include "System.h"

#include <FreeRTOS.h>
#include <task.h>

void logHeap() {
	/*  INFO(" heap:%d max_block:%d stack:%d ", heap_caps_get_free_size(MALLOC_CAP_32BIT),
	      heap_caps_get_largest_free_block(MALLOC_CAP_32BIT), uxTaskGetStackHighWaterMark(NULL));*/
	INFO("  free heap : %d stack:%d ", xPortGetFreeHeapSize(), uxTaskGetStackHighWaterMark(NULL));
}

System::System(ActorRef& mqtt)
	: _led1(DigitalOut::create(16))
	, _led2(DigitalOut::create(2))
	,_mqtt(mqtt) {
}

System::~System() {}

void System::preStart() {
	_led1.init();
	_led2.init();
	_reportTimer = timers().startPeriodicTimer("REPORT_TIMER", Msg("reportTimer"), 5000);
	_ledTimer = timers().startPeriodicTimer("LED_TIMER", Msg("ledTimer"), 100);
	eb.subscribe(self(), MessageClassifier(_mqtt,Mqtt::Disconnected));
	eb.subscribe(self(), MessageClassifier(_mqtt,Mqtt::Connected));
}

Receive& System::createReceive() {
	return receiveBuilder()
	.match(MsgClass::ReceiveTimeout(), [this](Msg& msg) { INFO(" No more messages since some time "); })

	.match(LABEL("ledTimer"),[this](Msg& msg) {
		static bool ledOn = false;
		_led1.write(ledOn ? 1 : 0);
		_led2.write(ledOn ? 1 : 0);
		ledOn = !ledOn;
	})
	.match(LABEL("reportTimer"),[this](Msg& msg) {logHeap();})

	.match(Mqtt::Connected,	[this](Msg& msg) { INFO(" MQTT Connected "); timers().find(_ledTimer)->interval(500); })

	.match( Mqtt::Disconnected,	[this](Msg& msg) { INFO(" MQTT Disconnected "); timers().find(_ledTimer)->interval(100); })

	.match(MsgClass::Properties(),[this](Msg& msg) {
		static uint32_t heap;
		static int32_t deltaHeap;
		deltaHeap = xPortGetFreeHeapSize();
		deltaHeap -= heap;
		sender().tell(replyBuilder(msg)
		              ("build",__DATE__ " " __TIME__)
		              ("cpu","ESP8266")
		              ("procs",1)
		              ("upTime",Sys::millis())
		              ("deltaHeap",deltaHeap)
		              ("heap",xPortGetFreeHeapSize())
		              ("stack",(uint32_t)uxTaskGetStackHighWaterMark(NULL))
		              ,self());
		heap=xPortGetFreeHeapSize();
	})
	.build();
}
