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
#include <Native.cpp>
#include <Echo.cpp>
#include <Bridge.cpp>
#include <Mqtt.h>
#include <Sender.cpp>
#include <Publisher.cpp>
//#include <ConfigActor.cpp>
#include <System.h>
#include <Wifi.h>
#include <RtosQueue.h>

#include <Config.h>
#include <DWM1000.h>
#include <DWM1000_Tag.h>
#include <DWM1000_Anchor.h>
#include <LogIsr.h>

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

/*
extern void* pxCurrentTCB;

void* tcb=0;
void setTask() {
	tcb=pxCurrentTCB;
	INFO("tcb = %X",tcb);

}
bool isTask() {
	return tcb==pxCurrentTCB;
}*/
/*

static void  mqtt_task(void *pvParameters) {
	INFO(" MQTT task started [%X]",pxCurrentTCB);
	setTask();
	MessageDispatcher* dispatcher = (MessageDispatcher*)pvParameters;

//	dispatcher->execute();
}

void akkaMainTask(void* pvParameter) {

}*/
#include <espressif/esp_system.h>
extern "C" void user_init(void) {
	uart_set_baud(0, 921600);
	sdk_system_update_cpu_freq(SYS_CPU_160MHZ); // need for speed, DWM1000 doesn't wait !
	Sys::init();

	printf("Starting Akka on %s heap : %d ", Sys::getProcessor(), Sys::getFreeHeap());
	static MessageDispatcher defaultDispatcher( 1,  1024,tskIDLE_PRIORITY + 1);
	static ActorSystem actorSystem(Sys::hostname(), defaultDispatcher);

//	actorSystem.actorOf<Sender>("sender");
	ActorRef& wifi = actorSystem.actorOf<Wifi>("wifi");
	ActorRef& mqtt = actorSystem.actorOf<Mqtt>("mqtt",wifi);
	ActorRef& publisher = actorSystem.actorOf<Publisher>("publisher",mqtt);
	ActorRef& bridge = actorSystem.actorOf<Bridge>("bridge",mqtt);
	ActorRef& system = actorSystem.actorOf<System>("system",mqtt);
	actorSystem.actorOf<LogIsr>("logIsr");

    std::string role;
    config.setNameSpace("dwm1000");
    config.get("role",role,"N");
    if ( role.at(0)=='T' ) {
    	ActorRef& tag = actorSystem.actorOf<DWM1000_Tag>("tag",publisher,Spi::create(12,13,14,15),DigitalIn::create(4),DigitalOut::create(5),sdk_system_get_chip_id() & 0xFFFF,(uint8_t*)"ABCDEF");
    } else if ( role.at(0)=='A'  ) {
    	ActorRef& anchor = actorSystem.actorOf<DWM1000_Anchor>("anchor",publisher,Spi::create(12,13,14,15),DigitalIn::create(4),DigitalOut::create(5),sdk_system_get_chip_id() & 0xFFFF,(uint8_t*)"GHIJKL");
    }
    config.save();
}


