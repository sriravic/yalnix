/*
    Functions to handle the scheduler logic
*/

#include <process.h>

void scheduleSleepingProcesses()
{
    PCB* sleepingPCB = getHeadProcess(&gSleepBlockedQ);
    PCB* sleepingPCBNext;
	while(sleepingPCB != NULL) {
		sleepingPCB->m_timeToSleep--;
        sleepingPCBNext = sleepingPCB->m_next;
		// if the sleeping process wakes up, remove it from sleeping and move it to ready to run
		if(sleepingPCB->m_timeToSleep == 0) {
            processRemove(&gSleepBlockedQ, sleepingPCB);
			processEnqueue(&gReadyToRunProcessQ, sleepingPCB);
		}
        sleepingPCB = sleepingPCBNext;
	}
}
