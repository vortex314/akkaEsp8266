/*
 * LPS.h
 *
 *  Created on: Apr 18, 2019
 *      Author: lieven
 */

#ifndef COMPONENTS_LPS_LPS_H_
#define COMPONENTS_LPS_LPS_H_
#include <Akka.h>
#include <Hardware.h>
#include <Config.h>
#include <DWM1000.h>
#include <DWM1000_Tag.h>
#include <DWM1000_Anchor.h>

class LPS : public Actor {
		Label _reportTimer;
		uint32_t _interval = 100;
		ActorRef& _mqtt;

	public:
		LPS(ActorRef& mqtt);
		~LPS();
		void preStart();
		Receive& createReceive();
};
#endif /* COMPONENTS_LPS_LPS_H_ */
