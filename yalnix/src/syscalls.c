#include <process.h>
#include <yalnix.h>
#include <yalnixutils.h>
#include <stdbool.h>

// the global process id counter
extern int gPID;

// Fork handles the creation of a new process. It is the only way to create a new process in Yalnix
int kernelFork(void)
{
    // Get the current running process's pcb
    PCB* currpcb = getHeadProcess(&gRunningProcessQ);
    PCB* nextpcb = (PCB*)malloc(sizeof(PCB));
    PageTable* nextpt = (PageTable*)malloc(sizeof(PageTable));
    memset(nextpt, 0, sizeof(PageTable));
    PageTable* currpt = currpcb->m_pt;
    UserContext* nextuctx = (UserContext*)malloc(sizeof(UserContext));
    KernelContext* nextkctx = (KernelContext*)malloc(sizeof(KernelContext));

    if(nextpcb != NULL && nextpt != NULL && nextuctx != NULL /*&& nextkctx != NULL*/)
    {
        // initialize this pcb
        nextpcb->m_pid = gPID++;
        nextpcb->m_ppid = currpcb->m_pid;
        nextpcb->m_state = PROCESS_READY;
        nextpcb->m_ticks = 0;
        nextpcb->m_timeToSleep = 0;
        nextpcb->m_pt = nextpt;
        nextpcb->m_brk = currpcb->m_brk;
        memcpy(nextuctx, currpcb->m_uctx, sizeof(UserContext));
        //memcpy(nextkctx, currpcb->m_kctx, sizeof(KernelContext));
        nextpcb->m_uctx = nextuctx;
        nextpcb->m_kctx = nextkctx;

        // copy the entries of the region 0 up to kernel stack size
        int pg;
        int r0kernelPages = DOWN_TO_PAGE(KERNEL_STACK_BASE) / PAGESIZE;
        int r0StackPages = DOWN_TO_PAGE(KERNEL_STACK_LIMIT) / PAGESIZE;
        int r1Pages = DOWN_TO_PAGE(VMEM_LIMIT) / PAGESIZE;
        for(pg = 0; pg < r0kernelPages; pg++)
        {
            // copy r0 pages if only they are valid
            // plus they point to the same physical frames in memory
            // we don't copy the contents into new frames since all processes
            // share the same kernel r0 address space
            if(currpt->m_pte[pg].valid == 1)
            {
                nextpt->m_pte[pg].valid = 1;
                nextpt->m_pte[pg].prot = currpt->m_pte[pg].prot;
                nextpt->m_pte[pg].pfn = currpt->m_pte[pg].pfn;
            }
        }

        // add two pages for kernel stack since they are unique to each process.
        for(pg = r0kernelPages; pg < r0StackPages; pg++)
        {
            FrameTableEntry* frame = getOneFreeFrame(&gFreeFramePool, &gUsedFramePool);
            if(frame != NULL)
            {
                nextpt->m_pte[pg].valid = 1;
                nextpt->m_pte[pg].prot = currpt->m_pte[pg].prot;
                nextpt->m_pte[pg].pfn = frame->m_frameNumber;
            }
            else
            {
                TracePrintf(0, "Error allocating physical frames for the region0 of forked process");
            }
        }

        // Now process each region1 page
        // We first allocate one temporary page in R0
        // we then copy the contents of the R1(parent)  -> R0 page
        // we swap out the address space of the page tables of R1
        // we then copy from R0 page -> R1(child)
        // we repeat this process till we have copied all the pages appropriately
        unsigned int kernelBrk = (unsigned int)gKernelBrk;
        unsigned int kernelBrkPage = kernelBrk / PAGESIZE;

        FrameTableEntry* temporary = getOneFreeFrame(&gFreeFramePool, &gUsedFramePool);
        currpt->m_pte[kernelBrkPage].valid = 1;
        currpt->m_pte[kernelBrkPage].prot = PROT_READ|PROT_WRITE;
        currpt->m_pte[kernelBrkPage].pfn = temporary->m_frameNumber;
        unsigned int r1offset = NUM_VPN >> 1;
        for(pg = r0StackPages; pg < r1Pages; pg++)
        {
            if(currpt->m_pte[pg].valid == 1)
            {
                FrameTableEntry* frame = getOneFreeFrame(&gFreeFramePool, &gUsedFramePool);
                if(frame != NULL)
                {
                    nextpt->m_pte[pg].valid = 1;
                    nextpt->m_pte[pg].prot = PROT_READ|PROT_WRITE;
                    nextpt->m_pte[pg].pfn = frame->m_frameNumber;
                    unsigned int src = pg * PAGESIZE;
                    unsigned int dest = kernelBrkPage * PAGESIZE;

                    // copy from R1(pg) -> R0(kernelbrkpage)
                    memcpy(dest, src, PAGESIZE);

                    // swap out parent's R1 address space
                    WriteRegister(REG_PTBR1, (unsigned int)(nextpt->m_pte + r1offset));
                    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

                    // perform the copy from R1 -> R0(child) address space
                    memcpy(src, dest, PAGESIZE);

                    // give the correct permissions for this frame
                    nextpt->m_pte[pg].prot = currpt->m_pte[pg].prot;

                    // swap back in the parent's R1 address space
                    WriteRegister(REG_PTBR1, (unsigned int)(currpt->m_pte + r1offset));
                    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
                }
            }
        }

        // remove the temporary used frame
        freeOneFrame(&gFreeFramePool, &gUsedFramePool, temporary->m_frameNumber);
        currpt->m_pte[kernelBrkPage].valid = 0;
        free(temporary);

        // Add the process to the ready-to-run queue
        processEnqueue(&gReadyToRunProcessQ, nextpcb);

        // We have added the process to the ready-to-run queue
        // We have to return correct return codes
        currpcb->m_uctx->regs[0] = nextpcb->m_pid;
        nextpcb->m_uctx->regs[1] = 0;
        nextpcb->m_uctx->sp = currpcb->m_uctx->sp;
        nextpcb->m_uctx->pc = currpcb->m_uctx->pc;
        return SUCCESS;
    }
    else
    {
        TracePrintf(0, "Error creating PCB/pagetable for the fork child process");
        return ERROR;
    }
    return ERROR;
}

// Exec replaces the currently running process with a new program
int kernelExec(char *filename, char **argvec)
{
	// replace the calling process's text by the text pointed to by filename
	// allocate a new heap and stack
    // call the new process as main(argc, argv) where argc and argv are determined by **argvec
    return -1;
}

// Exit terminates the calling process
void kernelExit(int status) {
	// Move the calling process to the gDead list
	// Free all the memory associated with the process
    // Save the status

}

// Wait
int kernelWait(int *status_ptr) {
    PCB* currPCB = getHeadProcess(&gRunningProcessQ);
    ExitData* exitData = exitDataDequeue(currPCB->m_edQ);
    // find if the running process has children by trawling the process queues
    bool hasChildProcess =
        getChildOfPid(&gReadyToRunProcessQ, currPCB->m_pid) != NULL ||
        getChildOfPid(&gWaitProcessQ, currPCB->m_pid) != NULL ||
        getChildOfPid(&gSleepBlockedQ, currPCB->m_pid) != NULL;

    if (!hasChildProcess && exitData == NULL)
    {
        // no running children and no exited children
        *status_ptr = -1;
        return ERROR;
    }
    else if (exitData == NULL)
    {
        // no exited children but running children, so move it to gWaitProcessQ
        processEnqueue(&gWaitProcessQ, currPCB);
        processDequeue(&gRunningProcessQ);
        *status_ptr = -1;
        return ERROR;   // not sure what to return in this case
    }
    else
    {
        // success
        *status_ptr = exitData->m_status;
        return exitData->m_pid;
    }
}

int kernelGetPid(void) {
	// Find the pid of the calling process and return it
	PCB* currPCB = getHeadProcess(&gRunningProcessQ);
	return currPCB->m_pid;
}

// Brk raises or lowers the value of the process's brk to contain addr
int kernelBrk(void *addr)
{
    unsigned int newAddr = (unsigned int)addr;

    // cannot allocate Region 0 memory
    if(newAddr < VMEM_1_BASE)
    {
        return ERROR;
    }

    // get the pcb of the current running process that called for this brk
    // compute a couple of entries to make our lives easier
    PCB* currPCB = getHeadProcess(&gRunningProcessQ);
    PageTable* currPt = currPCB->m_pt;
    unsigned int currBrk = currPCB->m_brk;
    unsigned int brkPgNum = currBrk/PAGESIZE;
    TracePrintf(2, "The current brk page is: %d\n", brkPgNum);

    unsigned int delta;
    unsigned int pgDiff;
    int i;

    if(newAddr >= currBrk)
    {
        // allocate memory
        delta = (newAddr - currBrk);
        pgDiff = delta/PAGESIZE;

        /// TODO: error handling if we run out of free frames
        for(i = 1; i <= pgDiff; i++)
        {
            FrameTableEntry* frame = getOneFreeFrame(&gFreeFramePool, &gUsedFramePool);
            unsigned int pfn = frame->m_frameNumber;
            currPt->m_pte[brkPgNum+i].valid = 1; currPt->m_pte[brkPgNum+i].prot = PROT_READ | PROT_WRITE; currPt->m_pte[brkPgNum+i].pfn = pfn;
            TracePrintf(2, "The allocated page number is %d and the frame number is %d\n", brkPgNum+i, pfn);
        }
    }
    else
    {
        // deallocate memory
        delta = (currBrk - newAddr);
        pgDiff = delta/PAGESIZE;
        for(i = 0; i < pgDiff; i++)
        {
            currPt->m_pte[brkPgNum-i].valid = 0; currPt->m_pte[brkPgNum-i].prot = PROT_NONE;
            int pfn = currPt->m_pte[brkPgNum-i].pfn;
            freeOneFrame(&gFreeFramePool, &gUsedFramePool, pfn);
            TracePrintf(2, "The freed page number is %d and the frame number is %d\n", brkPgNum-i, pfn);
        }

        // SRINATH: Why are we returning a SUCCESS here instead of a global success value?
        //          What makes this else branch special?
        return SUCCESS;
    }

    TracePrintf(2, "The page difference is: %d\n", pgDiff);

    // set the brk to be the new address
    currPCB->m_brk = newAddr;

    // SRINATH: Well we are returning a success code here also? why two separate returns?
    return SUCCESS;
}

// Delay pauses the process for a time of clock_ticks
int kernelDelay(int clock_ticks)
{
    TracePrintf(0, "kernalDelay called\n");
    if(clock_ticks < 0)
    {
        return ERROR;
    }
    else if (clock_ticks == 0)
    {
      return SUCCESS;
    }
    else
    {
        // Move the running process to the sleep queue
        PCB* currPCB = getHeadProcess(&gRunningProcessQ);
        currPCB->m_timeToSleep = clock_ticks;
        processEnqueue(&gSleepBlockedQ, currPCB);
        processDequeue(&gRunningProcessQ);
        TracePrintf(2, "Process PID is %d\n", currPCB->m_pid);
        return SUCCESS;
    }
}

// TTYRead reads from the terminal tty_id
int kernelTtyRead(int tty_id, void *buf, int len)
{
	// Move the calling process to the gIOBlocked list
	// Wait for an interruptTtyReceive trap
		// When one is received, copy len bytes into buf
		// Check for clean data
	// Move the calling process from the gIOBlocked list to the gReadyToRun list
    //return the number of bytes copied into
    return -1;
}

// TTYWrite writes to the terminal tty_id
int kernelTtyWrite(int tty_id, void *buf, int len)
{
	// Move the calling process to the gIOBlocked list
	// Check for clean data in buf
	// If len is greater than TERMINAL_MAX_LINE
		// Trap to the hardware and call interruptTtyTransmit() as many times as needed to clear all the input
	// Else
		// Trap to the hardware and call interruptTtyTransmit() just once
    // Move the calling process from the gIOBlocked list to the gReadyToRun list
    return -1;
}

int kernelPipeInit(int *pipe_idp)
{
	// Create a new pipe with a unique id, owned by the calling process
    // Save the id into pipe_idp
    return -1;
}

int kernelPipeRead(int pipe_id, void *buf, int len)
{
	// Add the pipe referenced by pipe_id to the gReadPipeQueue
	// Wait for the bytes to be available at the pipe
	// Read bytes from the pipe
    // Return the number of bytes read
    return -1;
}

int kernelPipeWrite(int pipe_id, void *buf, int len)
{
	// Add the pipe referenced by pipe_id to the gWritePipeQueue
	// Write len bytes to the buffer
    // Return
    return -1;
}

int kernelLockInit(int *lock_idp)
{
	// Create a new lock with a unique id, owned by the calling process, and initially unlocked
	// Add the new lock to gLockQueue
    // Save its unique id into lock_idp
    return -1;
}

int kernelAcquire(int lock_id)
{
	// if the lock is free, update the lock's holder
    // Else, add the calling process to the lock referenced by lock_id's queue of waiting processes
    return -1;
}

int kernelRelease(int lock_id) {
	// If the caller doesn't own the lock, return an error
    // Otherwise unlock the lock and give it to the next waiting process if there is one
    return -1;
}

int kernelCvarInit(int *cvar_idp) {
	// Create a new cvar with a unique id, owned by the calling process
	// Add the cvar to the gCVarQueue list
    // Save the unique id into cvar_idp
    return -1;
}

int kernelCvarSignal(int cvar_id) {
	// Find the cvar referred to by cvar_id and notify the first process waiting on it
    // Wake up a waiter
    return -1;
}

int kernelCvarBroadcast(int cvar_id) {
	// Find the cvar referred to by cvar_id and notify all processes waiting on it
    // Wake up a waiter
    return -1;
}

int kernelCvarWait(int cvar_id, int lock_id) {
	// Release the lock referenced by lock_id
	// Add the calling process to the waiting queue for cvar_id
	// Wait to be notified by a CvarSignal or CvarBroadcast
    // Acquire the lock again
    return -1;
}

int kernelReclaim(int id) {
	// If the calling process is not the owner of the lock/cvar/pipe referenced by id, return ERROR
	// Free all resources held by the lock/cvar/pipe (usually waiting queues)
    // Remove the lock/cvar/pipe from its global list
    return -1;
}
