/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <cstring>
#include <stdio.h>

#include <FreeRTOS.h>
#include <esp/uart.h>
#include <espressif/esp_system.h>

#include <task.h>

#include <Akka.cpp>
#include <Echo.cpp>
#include <Bridge.cpp>
#include <Mqtt.h>
#include <Sender.cpp>
#include <Publisher.cpp>
//#include <ConfigActor.cpp>
#include <System.h>
#include <Wifi.h>
#include <RtosQueue.h>

/******************************************************************************
 * FunctionName : app_main
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
 *******************************************************************************/

Log logger(256);
ActorMsgBus eb;

extern void XdrTester(uint32_t);

void vAssertCalled( unsigned long ulLine, const char * const pcFileName ) {
	printf("Assert called on : %s:%lu",pcFileName,ulLine);
}

extern "C" void vApplicationMallocFailedHook() {
	WARN(" malloc failed ! ");
	exit(-1);
}


extern void* pxCurrentTCB;

void* tcb=0;
void setTask() {
	tcb=pxCurrentTCB;
	INFO("tcb = %X",tcb);

}
bool isTask() {
	return tcb==pxCurrentTCB;
}


static void  mqtt_task(void *pvParameters) {
	INFO(" MQTT task started [%X]",pxCurrentTCB);
	setTask();
	MessageDispatcher* dispatcher = (MessageDispatcher*)pvParameters;

//	dispatcher->execute();
}

void akkaMainTask(void* pvParameter) {

}
extern "C" void user_init(void) {
	uart_set_baud(0, 115200);
	Sys::init();

	printf("Starting Akka on %s heap : %d ", Sys::getProcessor(), Sys::getFreeHeap());
	static Mailbox defaultMailbox("default", 100);
	static MessageDispatcher defaultDispatcher( 2,  1024,tskIDLE_PRIORITY + 1);
	defaultDispatcher.attach(defaultMailbox);
	static ActorSystem actorSystem(Sys::hostname(), defaultDispatcher, defaultMailbox);

	actorSystem.actorOf<Sender>("sender");
	ActorRef wifi = actorSystem.actorOf<Wifi>("wifi");
	ActorRef mqtt = actorSystem.actorOf<Mqtt>("mqtt",wifi);
	defaultDispatcher.start();

	ActorRef publisher = actorSystem.actorOf<Publisher>("publisher",mqtt);
	ActorRef bridge = actorSystem.actorOf<Bridge>("bridge",mqtt);
	defaultDispatcher.unhandled(bridge.cell());

	ActorRef system = actorSystem.actorOf<System>("system",mqtt);
	//ActorRef config = actorSystem.actorOf<ConfigActor>("config");
}

//	static Mailbox mqttMailbox("mqtt", 100);
//	MessageDispatcher mqttDispatcher( 1,  1024,tskIDLE_PRIORITY + 1);
//	mqttDispatcher.attach(mqttMailbox);
//	mqttDispatcher.start();

/*	ActorRef mqtt = actorSystem.actorOf<Mqtt>(Props::create()
	                .withDispatcher(mqttDispatcher)
	                .withMailbox(mqttMailbox)
	                ,"mqtt",wifi);*/


//	xTaskCreate(&mqtt_task, "mqtt_task", 640, &mqttDispatcher, tskIDLE_PRIORITY + 3, NULL);

