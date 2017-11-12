#include <fcntl.h>
#include <hardware.h>
#include <load_info.h>
#include <process.h>
#include <pagetable.h>
#include <terminal.h>
#include <unistd.h>
#include <yalnix.h>
#include <yalnixutils.h>
#include <synchronization.h>

// the global process id counter
extern int gPID;
extern void* gTerminalBuffer[NUM_TERMINALS];

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
        nextpcb->m_uctx->regs[0] = 0;
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
int kernelExec(char *name, char **args)
{
    // Get the pcb of the calling process
    PCB* currpcb = getHeadProcess(&gRunningProcessQ);
    PageTable* currpt = currpcb->m_pt;

    if(currpcb != NULL && currpt != NULL)
    {
        // clear some entries in the pcb
        currpcb->m_state = PROCESS_READY;
        currpcb->m_ticks = 0;
        currpcb->m_timeToSleep = 0;

        // R1 region setup code
        int fd;
        int (*entry)();
        struct load_info li;
        int i;
        char *cp;
        char **cpp;
        char *cp2;
        int argcount;
        int size;
        int text_pg1;
        int data_pg1;
        int data_npg;
        int stack_npg;
        long segment_size;
        char *argbuf;

        /*
        * Open the executable file
        */
         if ((fd = open(name, O_RDONLY)) < 0) {
             TracePrintf(0, "LoadProgram: can't open file '%s'\n", name);
             return ERROR;
         }

         if (LoadInfo(fd, &li) != LI_NO_ERROR) {
             TracePrintf(0, "LoadProgram: '%s' not in Yalnix format\n", name);
             close(fd);
             return (-1);
         }

         if (li.entry < VMEM_1_BASE) {
             TracePrintf(0, "LoadProgram: '%s' not linked for Yalnix\n", name);
             close(fd);
             return ERROR;
         }

         /*
         * Figure out in what region 1 page the different program sections
         * start and end
         */
         text_pg1 = (li.t_vaddr - VMEM_1_BASE) >> PAGESHIFT;
         data_pg1 = (li.id_vaddr - VMEM_1_BASE) >> PAGESHIFT;
         data_npg = li.id_npg + li.ud_npg;
         /*
         *  Figure out how many bytes are needed to hold the arguments on
         *  the new stack that we are building.  Also count the number of
         *  arguments, to become the argc that the new "main" gets called with.
         */
         size = 0;
         for (i = 0; args[i] != NULL; i++)
         {
             TracePrintf(3, "counting arg %d = '%s'\n", i, args[i]);
             size += strlen(args[i]) + 1;
         }
         argcount = i;

         TracePrintf(2, "LoadProgram: argsize %d, argcount %d\n", size, argcount);

         /*
         *  The arguments will get copied starting at "cp", and the argv
         *  pointers to the arguments (and the argc value) will get built
         *  starting at "cpp".  The value for "cpp" is computed by subtracting
         *  off space for the number of arguments (plus 3, for the argc value,
         *  a NULL pointer terminating the argv pointers, and a NULL pointer
         *  terminating the envp pointers) times the size of each,
         *  and then rounding the value *down* to a double-word boundary.
         */
         cp = ((char *)VMEM_1_LIMIT) - size;

         cpp = (char **)
             (((int)cp -
             ((argcount + 3 + POST_ARGV_NULL_SPACE) *sizeof (void *)))
             & ~7);

         /*
         * Compute the new stack pointer, leaving INITIAL_STACK_FRAME_SIZE bytes
         * reserved above the stack pointer, before the arguments.
         */
         cp2 = (caddr_t)cpp - INITIAL_STACK_FRAME_SIZE;

         TracePrintf(1, "prog_size %d, text %d data %d bss %d pages\n",
         li.t_npg + data_npg, li.t_npg, li.id_npg, li.ud_npg);

         /*
         * Compute how many pages we need for the stack */
         stack_npg = (VMEM_1_LIMIT - DOWN_TO_PAGE(cp2)) >> PAGESHIFT;

         TracePrintf(1, "LoadProgram: heap_size %d, stack_size %d\n",
             li.t_npg + data_npg, stack_npg);


         /* leave at least one page between heap and stack */
         if (stack_npg + data_pg1 + data_npg >= MAX_PT_LEN)
         {
             close(fd);
             return ERROR;
         }

         /*
         * This completes all the checks before we proceed to actually load
         * the new program.  From this point on, we are committed to either
         * loading succesfully or killing the process.
         */

         /*
         * Set the new stack pointer value in the process's exception frame.
         */
         currpcb->m_uctx->sp = cp2;

         /*
         * Now save the arguments in a separate buffer in region 0, since
         * we are about to blow away all of region 1.
         */
         cp2 = argbuf = (char *)malloc(size);
         if(cp2 != NULL)
         {
             for (i = 0; args[i] != NULL; i++)
             {
                 TracePrintf(3, "saving arg %d = '%s'\n", i, args[i]);
                 strcpy(cp2, args[i]);
                 cp2 += strlen(cp2) + 1;
             }
         }
         else
         {
             TracePrintf(0, "Unable to allocate space for new program for arguments\n");
             return ERROR;
         }

         /*
         * Set up the page tables for the process so that we can read the
         * program into memory.  Get the right number of physical pages
         * allocated, and set them all to writable.
         */
         PageTable* pt = currpt;

         // invalidate all the pages for region 1
         // R1 starts from VMEM_1_BASE >> 1 till NUM_VPN
         freeRegionOneFrames(currpcb);

         // Allocate "li.t_npg" physical pages and map them starting at
         // the "text_pg1" page in region 1 address space.
         // These pages should be marked valid, with a protection of
         // (PROT_READ | PROT_WRITE).
         int allocPages = 0;
         int pg;
         unsigned int r1offset = (VMEM_1_BASE) / PAGESIZE;
         for(pg = text_pg1 + r1offset; pg < NUM_VPN && allocPages < li.t_npg; pg++)
         {
             pt->m_pte[pg].valid = 1;
             pt->m_pte[pg].prot = PROT_READ | PROT_WRITE;
             FrameTableEntry* frame = getOneFreeFrame(&gFreeFramePool, &gUsedFramePool);
             pt->m_pte[pg].pfn = frame->m_frameNumber;
             allocPages++;
         }

         // Allocate "data_npg" physical pages and map them starting at
         // the  "data_pg1" in region 1 address space.
         // These pages should be marked valid, with a protection of
         // (PROT_READ | PROT_WRITE).
         allocPages = 0;
         for(pg = data_pg1 + r1offset; pg < NUM_VPN && allocPages < data_npg; pg++)
         {
             pt->m_pte[pg].valid = 1;
             pt->m_pte[pg].prot = PROT_READ | PROT_WRITE;
             FrameTableEntry* frame = getOneFreeFrame(&gFreeFramePool, &gUsedFramePool);
             pt->m_pte[pg].pfn = frame->m_frameNumber;
             allocPages++;
         }

         // set the brk of the heap to be the base address of the next page above datasegment
         currpcb->m_brk = pg * PAGESIZE;

         /*
         * Allocate memory for the user stack too.
         */
         // Allocate "stack_npg" physical pages and map them to the top
         // of the region 1 virtual address space.
         // These pages should be marked valid, with a
         // protection of (PROT_READ | PROT_WRITE).
         allocPages = 0;
         for(pg = NUM_VPN - 1; pg > r1offset && allocPages < stack_npg; pg--)
         {
             pt->m_pte[pg].valid = 1;
             pt->m_pte[pg].prot = PROT_READ | PROT_WRITE;
             FrameTableEntry* frame = getOneFreeFrame(&gFreeFramePool, &gUsedFramePool);
             pt->m_pte[pg].pfn = frame->m_frameNumber;
             allocPages++;
         }

         /*
         * All pages for the new address space are now in the page table.
         * But they are not yet in the TLB, remember!
         */
         WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

         /*
         * Read the text from the file into memory.
         */
         lseek(fd, li.t_faddr, SEEK_SET);
         segment_size = li.t_npg << PAGESHIFT;
         int code = read(fd, (void *) li.t_vaddr, segment_size);
         if (code != segment_size)
         {
             close(fd);
             TracePrintf(0, "Copy program text failed\n");
             // KILL is not defined anywhere: it is an error code distinct
             // from ERROR because it requires different action in the caller.
             // Since this error code is internal to your kernel, you get to define it.
             return KILL;
         }

         /*
         * Read the data from the file into memory.
         */
         lseek(fd, li.id_faddr, 0);
         segment_size = li.id_npg << PAGESHIFT;

         if (read(fd, (void *) li.id_vaddr, segment_size) != segment_size)
         {
             close(fd);
             TracePrintf(0, "Copy program data failed\n");
             return KILL;
         }

         /*
         * Now set the page table entries for the program text to be readable
         * and executable, but not writable.
         */

         // Change the protection on the "li.t_npg" pages starting at
         // virtual address VMEM_1_BASE + (text_pg1 << PAGESHIFT).  Note
         // that these pages will have indices starting at text_pg1 in
         // the page table for region 1.
         // The new protection should be (PROT_READ | PROT_EXEC).
         // If any of these page table entries is also in the TLB, either
         // invalidate their entries in the TLB or write the updated entries
         // into the TLB.  It's nice for the TLB and the page tables to remain
         // consistent.
         for(pg = r1offset; pg < r1offset + li.t_npg; pg++)
         {
             pt->m_pte[pg].prot = PROT_READ | PROT_EXEC;
         }

         // flush region1 TLB
         WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

         close(fd);			/* we've read it all now */

         /*
         * Zero out the uninitialized data area
         */
         bzero(li.id_end, li.ud_end - li.id_end);

         /*
         * Set the entry point in the exception frame.
         */
         currpcb->m_uctx->pc = (caddr_t) li.entry;

         /*
         * Now, finally, build the argument list on the new stack.
         */
         #ifdef LINUX
         memset(cpp, 0x00, VMEM_1_LIMIT - ((int) cpp));
         #endif

         *cpp++ = (char *)argcount;		/* the first value at cpp is argc */
         cp2 = argbuf;
         for (i = 0; i < argcount; i++)
         {
             /* copy each argument and set argv */
             *cpp++ = cp;
             strcpy(cp, cp2);
             cp += strlen(cp) + 1;
             cp2 += strlen(cp2) + 1;
         }
         free(argbuf);
         *cpp++ = NULL;			/* the last argv is a NULL pointer */
         *cpp++ = NULL;			/* a NULL pointer for an empty envp */

         // reset the pagetables to the calling process
         WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
         return SUCCESS;
    }
}

// Exit terminates the calling process
void kernelExit(int status)
{
    PCB* currPCB = getHeadProcess(&gRunningProcessQ);
    if(currPCB->m_pid == 0)
    {
        // If init exits, halt the system
        Halt();
    }

    // get the parent PCB of the calling process if it exists
    PCB* parentPCB = getPcbByPid(&gReadyToRunProcessQ, currPCB->m_ppid);
    if(parentPCB == NULL)
    {
        parentPCB = getPcbByPid(&gSleepBlockedQ, currPCB->m_ppid);
    }
    if(parentPCB == NULL)
    {
        parentPCB = getPcbByPid(&gWaitProcessQ, currPCB->m_ppid);
    }

    // if the process has a parent, save its exit data into its parents list
    if(parentPCB != NULL)
    {
        // create the exit data struct
        ExitData* exitData = (ExitData*)malloc(sizeof(ExitData));
        if(exitData == NULL)
        {
            TracePrintf(0, "Failed to malloc for exit data\n");
            exit(-1);
        }
        exitData->m_status = status;
        exitData->m_pid = currPCB->m_pid;

        // put the exit data into the parents exit data queue
        exitDataEnqueue(parentPCB->m_edQ, exitData);
    }
}

// Wait
int kernelWait(int *status_ptr) {
    PCB* currPCB = getHeadProcess(&gRunningProcessQ);
    ExitData* exitData = exitDataDequeue(currPCB->m_edQ);
    // find if the running process has children by trawling the process queues
    bool hasChildProcess =
        getChildOfPpid(&gReadyToRunProcessQ, currPCB->m_pid) != NULL ||
        getChildOfPpid(&gWaitProcessQ, currPCB->m_pid) != NULL ||
        getChildOfPpid(&gSleepBlockedQ, currPCB->m_pid) != NULL;

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
    PCB* currpcb = getHeadProcess(&gRunningProcessQ);
    addTerminalReadRequest(currpcb, tty_id, TERM_REQ_READ, buf, len);
    return SUCCESS;
}

// TTYWrite writes to the terminal tty_id
int kernelTtyWrite(int tty_id, void *buf, int len)
{
    // Add a request to the terminal queue
    PCB* currpcb = getHeadProcess(&gRunningProcessQ);
    addTerminalWriteRequest(currpcb, tty_id, TERM_REQ_WRITE, buf, len);
    return SUCCESS;
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

// Create a new lock with a unique id, owned by the calling process, and initially unlocked
// Add the new lock to gLockQueue
// Save its unique id into lock_idp
int kernelLockInit(int *lock_idp)
{
    PCB* currPCB = getHeadProcess(&gRunningProcessQ);

    *lock_idp = createLock(currPCB->m_pid);
    if(*lock_idp == -1)
    {
        return ERROR;
    }
    return SUCCESS;
}

int kernelAcquire(int lock_id)
{
    PCB* currPCB = getHeadProcess(&gRunningProcessQ);

    LockQueueNode* lockNode = getLockNode(lock_id);
    if(lockNode == NULL)
    {
        return ERROR;
    }

    Lock* lock = lockNode->m_pLock;
    if(lock->m_state == UNLOCKED)
    {
        // if the lock is free, update the lock's holder
        lock->m_owner = currPCB->m_pid;
        lock->m_state = LOCKED;
    }
    else
    {
        // Else, add the calling process to the lock referenced by lock_id's queue of waiting processes
        processDequeue(&gRunningProcessQ);
        lockWaitingEnqueue(lockNode, currPCB);
    }
    return SUCCESS;
}

int kernelRelease(int lock_id) {
    PCB* currPCB = getHeadProcess(&gRunningProcessQ);

    LockQueueNode* lockNode = getLockNode(lock_id);
    if(lockNode == NULL)
    {
        return ERROR;
    }

    Lock* lock = lockNode->m_pLock;
    if(lock->m_owner != currPCB->m_pid)
    {
        // If the caller doesn't own the lock, return an error
        return ERROR;
    }
    else
    {
        // Otherwise unlock the lock and give it to the next waiting process if there is one
        lock->m_state = UNLOCKED;
        lock->m_owner = -1;
        PCB* newLockOwner = lockWaitingDequeue(lockNode);
        if(newLockOwner != NULL)
        {
            lock->m_owner = newLockOwner->m_pid;
            lock->m_state = LOCKED;
            processEnqueue(&gReadyToRunProcessQ, newLockOwner);
        }
        return SUCCESS;
    }
}

int kernelCvarInit(int *cvar_idp) {
	// Create a new cvar with a unique id, owned by the calling process
	// Add the cvar to the gCVarQueue list
    // Save the unique id into cvar_idp
    PCB* currPCB = getHeadProcess(&gRunningProcessQ);
    *cvar_idp = createCVar(currPCB->m_pid);
    if(*cvar_idp == ERROR)
    {
        return ERROR;
    }
    return SUCCESS;
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
    int rc = kernelRelease(lock_id);
    if(rc == ERROR)
    {
        return ERROR;
    }

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
