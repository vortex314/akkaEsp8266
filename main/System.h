#ifndef SYSTEM_H
#define SYSTEM_H

#include <Akka.h>
#include <Hardware.h>

class System : public Actor
{
    Uid _ledTimer;
    Uid _reportTimer;
    DigitalOut& _ledGpio;
    DigitalOut& _relaisGpio;

    uint32_t _interval = 100;
    Uid _relaisTimer;

public:
    System(va_list args);
    ~System();
    void preStart();
    Receive& createReceive();
};

#endif // SYSTEM_H
