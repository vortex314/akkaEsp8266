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
#include <System.h>
#include <Wifi.h>

/******************************************************************************
 * FunctionName : app_main
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
 *******************************************************************************/

Log logger(256);
ActorMsgBus eb;

extern void XdrTester(uint32_t);

static void  mqtt_task(void *pvParameters) {
	MessageDispatcher* dispatcher = (MessageDispatcher*)pvParameters;

	dispatcher->execute();

}

void akkaMainTask(void* pvParameter) {
	uart_set_baud(0, 115200);
	Sys::delay(5000);
	Sys::init();
	INFO("Starting Akka on %s heap : %d ", Sys::getProcessor(), Sys::getFreeHeap());
	INFO("%s", Sys::getBoard());
	Sys::init();

	Mailbox defaultMailbox("default", 2048);
	Mailbox mqttMailbox("mqtt", 2048);

	MessageDispatcher defaultDispatcher;
	MessageDispatcher mqttDispatcher;

	ActorSystem actorSystem(Sys::hostname(), defaultDispatcher, defaultMailbox);

	ActorRef sender = actorSystem.actorOf<Sender>("Sender");
	ActorRef wifi = actorSystem.actorOf<Wifi>("Wifi");

	ActorRef mqtt = actorSystem.actorOf<Mqtt>(Props::create()
	                .withDispatcher(mqttDispatcher)
	                .withMailbox(mqttMailbox)
	                ,"Mqtt",wifi);

	ActorRef bridge = actorSystem.actorOf<Bridge>("Bridge",mqtt);
	ActorRef system = actorSystem.actorOf<System>("System",mqtt);

	defaultDispatcher.attach(defaultMailbox);
	defaultDispatcher.unhandled(bridge.cell());
	mqttDispatcher.attach(mqttMailbox);

	xTaskCreate(&mqtt_task, "mqtt_task", 1024, &mqttDispatcher, tskIDLE_PRIORITY + 1, NULL);
	defaultDispatcher.execute();

}
extern "C" void user_init(void) {
	uart_set_baud(0, 115200);
	xTaskCreate(&akkaMainTask, "akkaMainTask", 2000, NULL, tskIDLE_PRIORITY + 2, NULL);
}
