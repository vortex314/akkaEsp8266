#ifndef SYSTEM_H
#define SYSTEM_H

#include <Akka.h>
#include <Hardware.h>
#include <Mqtt.h>

class System : public Actor {
		Uid _ledTimer;
		Uid _reportTimer;
		DigitalOut& _led;
		DigitalOut& _relais;
		DigitalOut& _led1;
		DigitalOut& _led2;

		uint32_t _interval = 100;
		Uid _relaisTimer;
		ActorRef _mqtt;

	public:
		System(va_list args);
		~System();
		void preStart();
		Receive& createReceive();
};

#endif // SYSTEM_H
