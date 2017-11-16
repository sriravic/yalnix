/*
    Functions to handle the scheduler logic
*/

#include <process.h>
#include <yalnix.h>

extern KernelContext* SwitchKCS(KernelContext* kc_in, void* curr_pcb_p, void* next_pcb_p);

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

int scheduler(PCBQueue* destQueue, PCB* currpcb, UserContext* ctx, char* errormessage)
{
    processDequeue(&gRunningProcessQ);
    processEnqueue(destQueue, currpcb);

    PCB* nextpcb = getHeadProcess(&gReadyToRunProcessQ);
    memcpy(currpcb->m_uctx, ctx, sizeof(UserContext));
    if(nextpcb != NULL)
    {
        int rc = KernelContextSwitch(SwitchKCS, currpcb, nextpcb);
        if(rc == -1)
        {
            TracePrintf(SEVERE, "Kernel Context switch failed\n");
            exit(-1);
        }
    }
    else
    {
        TracePrintf(SEVERE, "ERROR: no pcb in %c\n", errormessage);
        Halt();
    }

    processRemove(&gReadyToRunProcessQ, currpcb);
    processEnqueue(&gRunningProcessQ, currpcb);
    currpcb->m_ticks = 0;

    // swap out the page tables
    swapPageTable(currpcb);
    memcpy(ctx, currpcb->m_uctx, sizeof(UserContext));
    return SUCCESS;
}
