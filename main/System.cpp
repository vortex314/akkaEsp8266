#include "System.h"

#include <FreeRTOS.h>
#include <task.h>

void logHeap()
{
    /*  INFO(" heap:%d max_block:%d stack:%d ", heap_caps_get_free_size(MALLOC_CAP_32BIT),
          heap_caps_get_largest_free_block(MALLOC_CAP_32BIT), uxTaskGetStackHighWaterMark(NULL));*/
    INFO("  free heap : %d stack:%d ", xPortGetFreeHeapSize(), uxTaskGetStackHighWaterMark(NULL));
}

System::System(va_list args)
    : _ledGpio(DigitalOut::create(2))
{
}

System::~System() {}

void System::preStart()
{
    _ledGpio.init();
    _reportTimer = timers().startPeriodicTimer("REPORT_TIMER", TimerExpired, 1000);
    _ledTimer = timers().startPeriodicTimer("LED_TIMER", TimerExpired, 100);
}

Receive& System::createReceive()
{
    return receiveBuilder()
        .match(ReceiveTimeout, [this](Envelope& msg) { INFO(" No more messages since some time "); })
        .match(TimerExpired,
            [this](Envelope& msg) {
	        uint16_t k;
	        msg.scanf("i", &k);
	        if(Uid(k) == _ledTimer) {
	            static bool ledOn = false;
	            _ledGpio.write(ledOn ? 1 : 0);
	            ledOn = !ledOn;
	        } else if(Uid(k) == _reportTimer) {
	            logHeap();
	        }
            })
        .build();
}