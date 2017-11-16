/* Header file for scheduler logic

*/

#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

void scheduleSleepingProcesses();

int scheduler(PCBQueue* destQueue, PCB* currpcb, UserContext* ctx, char* errormessage);

#endif
