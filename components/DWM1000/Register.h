/*
 * Register.h
 *
 *  Created on: 25 Apr 2019
 *      Author: lieven
 */

#ifndef COMPONENTS_DWM1000_REGISTER_H_
#define COMPONENTS_DWM1000_REGISTER_H_
#include <stdint.h>
#include <string>
#include <Log.h>
class Register {
	const char* _name;
	uint32_t _reg;
	const char* _format;
public:
	Register(const char* name,const char* format);
	virtual ~Register();
	void value(uint32_t);
	void show();
};

const char* fmt1 =
		"ICRBP HSRBP AFFREJ TXBERR HPDWARN RXSFDTO CLKPLL_LL RFPLL_LL "
		"SLP2INIT GPIOIRQ RXPTO RXOVRR F LDEERR RXRFTO RXRFSL RXFCE RXFCG "
		"RXDFR RXPHE RXPHD LDEDONE RXSFDD RXPRD TXFRS TXPHS TXPRS TXFRB AAT "
		"ESYNCR CPLOCK IRQSD";

#endif /* COMPONENTS_DWM1000_REGISTER_H_ */
