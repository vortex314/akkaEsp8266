#ifndef SYSTEM_H
#define SYSTEM_H

#include <Akka.h>
#include <Hardware.h>
#include <Mqtt.h>
#include <Wifi.h>

class System : public Actor {
		Label _led1Timer;
		Label _led2Timer;
		DigitalOut& _led1;
		DigitalOut& _led2;

		ActorRef& _mqtt;
		ActorRef& _wifi;

	public:
		System(ActorRef& mqtt,ActorRef& wifi);
		~System();
		void preStart();
		Receive& createReceive();
};

#endif // SYSTEM_H
