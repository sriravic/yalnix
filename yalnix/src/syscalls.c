#include <process.h>
#include <yalnix.h>

// Fork handles the creation of a new process. It is the only way to create a new process in Yalnix
int kernelFork(void) {
	// copy the parent process into a new child process, changing the pid
	// put the child process in the gReadyToRun list
    // ??? something about page tables ???
    TracePrintf(2, "Someone called kernelFork\n");
    return -1;
}

// Exec replaces the currently running process with a new program
int kernelExec(char *filename, char **argvec) {
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
	// If the calling process has no children or has a child that is already dead, return ERROR
	// Move the calling process to the gSyscallBlocked list
	// Wait for the processes child to Exit()
	// Save the child's exit status to status_ptr
	// Move the calling process from the gSyscallBlocked list to the gReadyToRun list
    // Return the childs pid
    return -1;
}

int kernelGetPid(void) {
	// Find the pid of the calling process and return it
	PCB* currPCB = getHeadProcess(gRunningProcessQ);
	return currPCB->m_pid;
}

// Brk raises or lowers the value of the process's brk to contain addr
int kernelBrk(void *addr) {
  unsigned int newAddr = (unsigned int)addr;

  // cannot allocate Region 0 memory
  if(newAddr < VMEM_1_BASE)
    return ERROR;

  PCB* currPCB = getHeadProcess(gRunningProcessQ);
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
    // TODO error handling if we run out of free frames
    for(i = 1; i <= pgDiff; i++) {
      FrameTableEntry* frame = getOneFreeFrame(&gFreeFramePool, &gUsedFramePool);
      unsigned int pfn = frame->m_frameNumber;
      currPt->m_pte[brkPgNum+i].valid = 1; currPt->m_pte[brkPgNum+i].prot = PROT_READ | PROT_WRITE; currPt->m_pte[brkPgNum+i].pfn = pfn;
      TracePrintf(2, "The allocated page number is %d and the frame number is %d\n", brkPgNum+i, pfn);
    }
  } else
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

    return SUCCESS;
  }

  TracePrintf(2, "The page difference is: %d\n", pgDiff);

  // set the brk to be the new address
  currPCB->m_brk = newAddr;
  return SUCCESS;
}

// Delay pauses the process for a time of clock_ticks
int kernelDelay(int clock_ticks) {
    TracePrintf(0, "kernalDelay called\n");
    if(clock_ticks < 0)
      return ERROR;
    else if (clock_ticks == 0)
      return SUCCESS;
    else {
      // Move the running process to the sleep queue
      PCB* currPCB = getHeadProcess(gRunningProcessQ);
      currPCB->m_timeToSleep = clock_ticks;
      processEnqueue(gSleepBlockedQ, currPCB);
      processDequeue(gRunningProcessQ);
      TracePrintf(2, "Process PID is %d\n", currPCB->m_pid);

      return SUCCESS;
    }
}

// TTYRead reads from the terminal tty_id
int kernelTtyRead(int tty_id, void *buf, int len) {
	// Move the calling process to the gIOBlocked list
	// Wait for an interruptTtyReceive trap
		// When one is received, copy len bytes into buf
		// Check for clean data
	// Move the calling process from the gIOBlocked list to the gReadyToRun list
    //return the number of bytes copied into
    return -1;
}

// TTYWrite writes to the terminal tty_id
int kernelTtyWrite(int tty_id, void *buf, int len) {
	// Move the calling process to the gIOBlocked list
	// Check for clean data in buf
	// If len is greater than TERMINAL_MAX_LINE
		// Trap to the hardware and call interruptTtyTransmit() as many times as needed to clear all the input
	// Else
		// Trap to the hardware and call interruptTtyTransmit() just once
    // Move the calling process from the gIOBlocked list to the gReadyToRun list
    return -1;
}

int kernelPipeInit(int *pipe_idp) {
	// Create a new pipe with a unique id, owned by the calling process
    // Save the id into pipe_idp
    return -1;
}

int kernelPipeRead(int pipe_id, void *buf, int len) {
	// Add the pipe referenced by pipe_id to the gReadPipeQueue
	// Wait for the bytes to be available at the pipe
	// Read bytes from the pipe
    // Return the number of bytes read
    return -1;
}

int kernelPipeWrite(int pipe_id, void *buf, int len) {
	// Add the pipe referenced by pipe_id to the gWritePipeQueue
	// Write len bytes to the buffer
    // Return
    return -1;
}

int kernelLockInit(int *lock_idp) {
	// Create a new lock with a unique id, owned by the calling process, and initially unlocked
	// Add the new lock to gLockQueue
    // Save its unique id into lock_idp
    return -1;
}

int kernelAcquire(int lock_id) {
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
