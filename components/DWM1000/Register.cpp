/*
 * Register.cpp
 *
 *  Created on: 25 Apr 2019
 *      Author: lieven
 */

#include "Register.h"

Register::Register(const char* name,const char* format) {
	_reg=0;
	_format=format;
	_name=name;
}

Register::~Register() {

}

void Register::value(uint32_t reg){_reg=reg;}

void Register::show(){
	std::string str;
	uint32_t reg=_reg;
	const char* fmt = _format;
	while ( fmt != 0 ) {
		const char* next = strchr(fmt,' ');
		if ( reg & 0x80000000 ) {
			if ( next==0) str.append(fmt);
			else str.append(fmt,next-fmt);
		}
		str.append(" ");
		fmt=next;
	}
	INFO(" %s = %s",_name, str.c_str());
}

