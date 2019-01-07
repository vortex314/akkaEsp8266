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
	_reportTimer = timers().startPeriodicTimer("REPORT_TIMER", Msg("reportTimer"), 5000);
	_ledTimer = timers().startPeriodicTimer("LED_TIMER", Msg("ledTimer"), 100);
	_relaisTimer = timers().startPeriodicTimer("RELAIS_TIMER", Msg("relaisTimer"), 100000);
	eb.subscribe(self(), MessageClassifier(_mqtt,Mqtt::Disconnected));
	eb.subscribe(self(), MessageClassifier(_mqtt,Mqtt::Connected));
}

Receive& System::createReceive() {
	return receiveBuilder()
	.match(ReceiveTimeout(), [this](Msg& msg) { INFO(" No more messages since some time "); })

	.match(MsgClass("ledTimer"),[this](Msg& msg) {
		static bool ledOn = false;
		_led.write(ledOn ? 1 : 0);
		_led1.write(ledOn ? 1 : 0);
		_led2.write(ledOn ? 1 : 0);
		ledOn = !ledOn;
	})
	.match(MsgClass("reportTimer"),[this](Msg& msg) {	logHeap();})
	.match(MsgClass("relaisTimer"),[this](Msg& msg) {
		INFO("relais");
		static bool relaisOn = false;
		_relais.write(relaisOn ? 1 : 0);
		relaisOn = !relaisOn;
	})

	.match(Mqtt::Connected,	[this](Msg& msg) { INFO(" MQTT Connected "); timers().find(_ledTimer)->interval(500); })

	.match( Mqtt::Disconnected,	[this](Msg& msg) { INFO(" MQTT Disconnected "); timers().find(_ledTimer)->interval(100); })

	.match(Properties(),[this](Msg& msg) {
		sender().tell(replyBuilder(msg)
		              ("build",__DATE__ " " __TIME__)
		              ("cpu","ESP8266")
		              ("procs",1)
		              ("upTime",Sys::millis())
		              ("ram",80*1024)
		              ("heap",xPortGetFreeHeapSize())
		              ("stack",(uint32_t)uxTaskGetStackHighWaterMark(NULL))
		              ,self());
	})
	.build();
}
