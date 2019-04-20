#ifndef SYSTEM_H
#define SYSTEM_H

#include <Akka.h>
#include <Hardware.h>
#include <Mqtt.h>

class System : public Actor {
		Label _ledTimer;
		Label _reportTimer;
		DigitalOut& _led1;
		DigitalOut& _led2;

		uint32_t _interval = 100;
		ActorRef& _mqtt;

	public:
		System(ActorRef& mqtt);
		~System();
		void preStart();
		Receive& createReceive();
};

#endif // SYSTEM_H
