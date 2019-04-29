#include "System.h"

#include <FreeRTOS.h>
#include <task.h>

MsgClass System::LedPulseOn("LedPulseOn");
MsgClass System::LedPulseOff("LedPulseOff");

System::System(ActorRef& mqtt, ActorRef& wifi)
		: _led1(DigitalOut::create(16)), _led2(DigitalOut::create(2)), _mqtt(mqtt), _wifi(wifi) {
}

System::~System() {
}

void System::preStart() {
	_led1.init();
	_led2.init();
	timers().startPeriodicTimer("REPORT_TIMER", Msg("reportTimer"), 10000);
	_led1Timer =
			timers().startPeriodicTimer("LED1_TIMER", Msg("led1Timer"), 100);
//	_led2Timer = timers().startPeriodicTimer("LED2_TIMER", Msg("led2Timer"), 100);

	eb.subscribe(self(), MessageClassifier(_mqtt, Mqtt::Disconnected));
	eb.subscribe(self(), MessageClassifier(_mqtt, Mqtt::Connected));
	eb.subscribe(self(), MessageClassifier(_wifi, Wifi::Connected));
	eb.subscribe(self(), MessageClassifier(_wifi, Wifi::Disconnected));

}

Receive& System::createReceive() {
	return receiveBuilder().match(MsgClass::ReceiveTimeout(), [this](Msg& msg) {INFO(" No more messages since some time ");})

	.match(LABEL("led1Timer"), [this](Msg& msg) {
		static bool led1On = false;
		_led1.write(led1On ? 1 : 0);
		led1On = !led1On;
	})

	.match(LedPulseOn, [this](Msg& msg) {
		INFO("%s",LedPulseOn.label());
		timers().startSingleTimer("LedPulseOff",Msg("LedPulseOff"),100);
		_led2.write( 1);
	})

	.match(LedPulseOff, [this](Msg& msg) {
		_led2.write(0);
	})

	.match(LABEL("reportTimer"), [this](Msg& msg) {
		INFO("  free heap : %d stack:%d ", xPortGetFreeHeapSize(), uxTaskGetStackHighWaterMark(NULL));
	})

	.match(Mqtt::Connected, [this](Msg& msg) {INFO("mqtt connected");timers().find(_led1Timer)->interval(500);})

	.match(Mqtt::Disconnected, [this](Msg& msg) {INFO("mqtt disconnected"); timers().find(_led1Timer)->interval(100);})

	.match(MsgClass::Properties(), [this](Msg& msg) {
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
	}).build();
}
