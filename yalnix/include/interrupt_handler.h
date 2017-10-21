/**
File: interrupt_handler.h

Authors: Team Zoidberg

Description: Contains the methods and structures for handling interrupts from hardware and user-land processes
*/
  
#ifndef __INTERRUPT_HANDLER_H__
#define __INTERRUPT_HANDLER_H__

#include <hardware.h>

extern void interruptKernel(UserContext* ctx);
extern void interruptClock(UserContext* ctx);
extern void interruptIllegal(UserContext* ctx);
extern void interruptMemory(UserContext* ctx);
extern void interruptMath(UserContext* ctx);
extern void interruptTtyReceive(UserContext* ctx);
extern void interruptTtyTransmit(UserContext* ctx);
extern void interruptDummy(UserContext* ctx);

#endif
