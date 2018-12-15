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
/*   ActorRef mqttBridge = actorSystem.actorOf<MqttBridge>(
         Props::create()
             .withMailbox(remoteMailbox)
             .withDispatcher(defaultDispatcher),
         "mqttBridge", "tcp://test.mosquitto.org:1883");*/
Log logger(256);
ActorMsgBus eb;

void akkaMainTask(void* pvParameter)
{
    uart_set_baud(0, 115200);
    Sys::init();
    //   while(true) { printf("SDK version:%s\n", sdk_system_get_sdk_version()); }
    INFO("Starting Akka on %s heap : %d ", Sys::getProcessor(), Sys::getFreeHeap());
    INFO("%s", Sys::getBoard());
    Sys::init();
    MessageDispatcher& defaultDispatcher = *new MessageDispatcher();
    Mailbox defaultMailbox = *new Mailbox("default", 2048, 256);
    Mailbox remoteMailbox = *new Mailbox("remote", 2048, 256);
    ActorSystem actorSystem(Sys::hostname(), defaultDispatcher, defaultMailbox);

    ActorRef sender = actorSystem.actorOf<Sender>("Sender");
    ActorRef system = actorSystem.actorOf<System>("System");
    ActorRef wifi = actorSystem.actorOf<Wifi>("Wifi");
    ActorRef mqtt = actorSystem.actorOf<Mqtt>("Mqtt");

    defaultDispatcher.attach(defaultMailbox);
    defaultDispatcher.attach(remoteMailbox);
    defaultDispatcher.unhandled(mqtt.cell());

    eb.subscribe(mqtt, EnvelopeClassifier(wifi, Wifi::Disconnected));
    eb.subscribe(mqtt, EnvelopeClassifier(wifi, Wifi::Connected));
    eb.subscribe(system, EnvelopeClassifier(mqtt, Mqtt::Connected));
    eb.subscribe(system, EnvelopeClassifier(mqtt, Mqtt::Disconnected));

    while(true) {
	defaultDispatcher.execute();
	if(defaultDispatcher.nextWakeup() > Sys::millis()) {
	    uint32_t delay = (defaultDispatcher.nextWakeup() - Sys::millis());
	    if(delay > 10) {
		if(delay > 10000) {
		    WARN(" big delay %u", delay);
		} else {
		    vTaskDelay(delay / 10); // is in 10 msec multiples
		}
	    }
	}
    }
}
extern "C" void user_init(void)
{
    uart_set_baud(0, 115200);
    xTaskCreate(&akkaMainTask, "akkaMainTask", 2000, NULL, tskIDLE_PRIORITY + 1, NULL);
}