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
extern KernelContext* GetKCS(KernelContext* kc_in, void* curr_pcb_p, void* next_pcb_p);
extern KernelContext* SwitchKCS(KernelContext* kc_in, void* curr_pcb_p, void* next_pcb_p);

// Fork handles the creation of a new process. It is the only way to create a new process in Yalnix
int kernelFork(void)
{
    // Get the current running process's pcb
    PCB* currpcb = getHeadProcess(&gRunningProcessQ);
    PCB* nextpcb = (PCB*)malloc(sizeof(PCB));
    memset(nextpcb, 0, sizeof(PCB));
    UserProgPageTable* nextpt = (UserProgPageTable*)malloc(sizeof(UserProgPageTable));
    memset(nextpt, 0, sizeof(UserProgPageTable));
    UserProgPageTable* currpt = currpcb->m_pagetable;
    UserContext* nextuctx = (UserContext*)malloc(sizeof(UserContext));

    if(nextpcb != NULL && nextpt != NULL && nextuctx != NULL)
    {
        // initialize this pcb
        nextpcb->m_pid = gPID++;
        nextpcb->m_ppid = currpcb->m_pid;
        nextpcb->m_state = PROCESS_READY;
        nextpcb->m_ticks = 0;
        nextpcb->m_timeToSleep = 0;
        nextpcb->m_pagetable = nextpt;
        nextpcb->m_brk = currpcb->m_brk;
        memcpy(nextuctx, currpcb->m_uctx, sizeof(UserContext));
        nextpcb->m_uctx = nextuctx;
        int pg;

        // Now process each region1 page
        // We first allocate one temporary page in R0
        // we then copy the contents of the R1(parent)  -> R0 page
        // we swap out the address space of the page tables of R1
        // we then copy from R0 page -> R1(child)
        // we repeat this process till we have copied all the pages appropriately

        // NOTE: The current fork might cause error when we are constructing the whole page table and frames
        //       We will have to undo the whole operation if we are not able to make the whole state valid

        // use the one page below the kernel stack for the temporary storage
        FrameTableEntry* temporary = getOneFreeFrame(&gFreeFramePool, &gUsedFramePool);
        gKernelPageTable.m_pte[gKStackPg0 - 1].valid = 1;
        gKernelPageTable.m_pte[gKStackPg0 - 1].prot = PROT_READ|PROT_WRITE;
        gKernelPageTable.m_pte[gKStackPg0 - 1].pfn = temporary->m_frameNumber;
        WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
        unsigned int r0offset = gNumPagesR0 * PAGESIZE;
        for(pg = 0; pg < gNumPagesR1; pg++)
        {
            if(currpt->m_pte[pg].valid == 1)
            {
                FrameTableEntry* frame = getOneFreeFrame(&gFreeFramePool, &gUsedFramePool);
                if(frame != NULL)
                {
                    nextpt->m_pte[pg].valid = 1;
                    nextpt->m_pte[pg].prot = PROT_READ|PROT_WRITE;
                    nextpt->m_pte[pg].pfn = frame->m_frameNumber;
                    unsigned int src = (pg * PAGESIZE) + r0offset;
                    unsigned int dest = (gKStackPg0 - 1) * PAGESIZE;

                    // copy from R1(pg) -> R0(kernelbrkpage)
                    memcpy(dest, src, PAGESIZE);

                    // swap out parent's R1 address space
                    WriteRegister(REG_PTBR1, (unsigned int)(nextpt->m_pte));
                    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

                    // perform the copy from R1 -> R0(child) address space
                    memcpy(src, dest, PAGESIZE);

                    // give the correct permissions for this frame
                    nextpt->m_pte[pg].prot = currpt->m_pte[pg].prot;

                    // swap back in the parent's R1 address space
                    WriteRegister(REG_PTBR1, (unsigned int)(currpt->m_pte));
                    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
                }
                else
                {
                    return ERROR;
                }
            }
        }

        // remove the temporary used frame
        freeOneFrame(&gFreeFramePool, &gUsedFramePool, temporary->m_frameNumber);
        gKernelPageTable.m_pte[gKStackPg0 - 1].valid = 0;
        free(temporary);

        // allocate two frames for kernel stack frame
        FrameTableEntry* kstack1 = getOneFreeFrame(&gFreeFramePool, &gUsedFramePool);
        FrameTableEntry* kstack2 = getOneFreeFrame(&gFreeFramePool, &gUsedFramePool);
        nextpcb->m_pagetable->m_kstack[0].valid = 1; nextpcb->m_pagetable->m_kstack[0].prot = PROT_READ | PROT_WRITE; nextpcb->m_pagetable->m_kstack[0].pfn = kstack1->m_frameNumber;
        nextpcb->m_pagetable->m_kstack[1].valid = 1; nextpcb->m_pagetable->m_kstack[1].prot = PROT_READ | PROT_WRITE; nextpcb->m_pagetable->m_kstack[1].pfn = kstack1->m_frameNumber;

        int rc = KernelContextSwitch(GetKCS, nextpcb, NULL);
        if(rc == -1)
        {
            TracePrintf(0, "ERROR: Unable to get kernel stack for child\n");
            return ERROR;
        }

        // We have to return correct return codes
        if(gRunningProcessQ.m_head == NULL)
        {
            // The child woke up suddenly and found it can start running.!
            TracePrintf(0, "INFO: Waking up as the child.\n");
            swapPageTable(nextpcb);
            nextpcb->m_uctx->regs[0] = 0;
            processRemove(&gReadyToRunProcessQ, nextpcb);
            processEnqueue(&gRunningProcessQ, nextpcb);
            return SUCCESS;
        }
        else
        {
            // The parent puts the child in the ready to run queue and goes out doing its thing
            currpcb->m_uctx->regs[0] = nextpcb->m_pid;
            processEnqueue(&gReadyToRunProcessQ, nextpcb);
        }
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
    UserProgPageTable* currpt = currpcb->m_pagetable;

    if(currpcb != NULL && currpt != NULL)
    {
        // clear some entries in the pcb
        currpcb->m_state = PROCESS_RUNNING;
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
         UserProgPageTable* pt = currpt;

         // invalidate all the pages for region 1
         // R1 starts from VMEM_1_BASE >> 1 till NUM_VPN
         freeRegionOneFrames(currpcb);

         // Allocate "li.t_npg" physical pages and map them starting at
         // the "text_pg1" page in region 1 address space.
         // These pages should be marked valid, with a protection of
         // (PROT_READ | PROT_WRITE).
         int pg;
         int allocPages = 0;
         for(pg = text_pg1; pg < gNumPagesR1 && allocPages < li.t_npg; pg++)
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
         for(pg = data_pg1; pg < gNumPagesR1 && allocPages < data_npg; pg++)
         {
             pt->m_pte[pg].valid = 1;
             pt->m_pte[pg].prot = PROT_READ | PROT_WRITE;
             FrameTableEntry* frame = getOneFreeFrame(&gFreeFramePool, &gUsedFramePool);
             pt->m_pte[pg].pfn = frame->m_frameNumber;
             allocPages++;
         }

         // set the brk of the heap to be the base address of the next page above datasegment
         currpcb->m_brk = (pg + gNumPagesR0) * PAGESIZE;

         /*
         * Allocate memory for the user stack too.
         */
         // Allocate "stack_npg" physical pages and map them to the top
         // of the region 1 virtual address space.
         // These pages should be marked valid, with a
         // protection of (PROT_READ | PROT_WRITE).
         allocPages = 0;
         for(pg = R1PAGES - 1; pg > 0 && allocPages < stack_npg; pg--)
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
         for(pg = 0; pg < li.t_npg; pg++)
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
    else
    {
        return ERROR;
    }
}

// Exit terminates the calling process
void kernelExit(int status)
{
    PCB* currPCB = getHeadProcess(&gRunningProcessQ);
    if(currPCB->m_pid == 0)
    {
        // If init exits, halt the system
        TracePrintf(0, "INIT EXITED SO HALTING THE SYSTEM\n");
        Halt();
    }

    // get the parent PCB of the calling process if it exists
    // TODO also need to search all lock waiting, cvar waiting, and ttyread waiting queues
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
            return;
        }
        exitData->m_status = status;
        exitData->m_pid = currPCB->m_pid;

        // put the exit data into the parents exit data queue
        exitDataEnqueue(parentPCB->m_edQ, exitData);
    }

    // free the pcb
    // TODO check that freePCB is up to date with any new stuff we added
    freePCB(currPCB);

    // half context switch because we will never come back

}

// Wait
int kernelWait(int *status_ptr, UserContext* ctx) {
    PCB* currpcb = getHeadProcess(&gRunningProcessQ);
    ExitData* exitData = exitDataDequeue(currpcb->m_edQ);
    // find if the running process has children by trawling the process queues
    bool hasChildProcess =
        getChildOfPpid(&gReadyToRunProcessQ, currpcb->m_pid) != NULL ||
        getChildOfPpid(&gWaitProcessQ, currpcb->m_pid) != NULL ||
        getChildOfPpid(&gSleepBlockedQ, currpcb->m_pid) != NULL;
        // TODO also need to search all lock waiting, cvar waiting, and ttyread waiting queues

    if (!hasChildProcess && exitData == NULL)
    {
        // no running children and no exited children
        *status_ptr = -1;
        return ERROR;
    }

    // temp code start
    // I think the while loop should only get run once ever. My thought here is that
    // if for some reason a process gets woken up erroneously, it will just go back to sleep.
    while(exitData == NULL)
    {
        TracePrintf(2, "Process waiting for a child to exit\n");

        // ******SCHEDULER******
        // no exited children but running children, so move it to gWaitProcessQ and context switch
        processDequeue(&gRunningProcessQ);
        processEnqueue(&gWaitProcessQ, currpcb);

        PCB* nextpcb = getHeadProcess(&gReadyToRunProcessQ);
        memcpy(currpcb->m_uctx, ctx, sizeof(UserContext));
        int rc = KernelContextSwitch(SwitchKCS, currpcb, nextpcb);
        if(rc == -1)
        {
            TracePrintf(0, "Kernel Context switch failed\n");
            exit(-1);
        }
        processRemove(&gReadyToRunProcessQ, currpcb);
        processEnqueue(&gRunningProcessQ, currpcb);
        currpcb->m_ticks = 0;

        // swap out the page tables
        swapPageTable(currpcb);
        memcpy(ctx, currpcb->m_uctx, sizeof(UserContext));
        // ******SCHEDULER******

        // when this process picks up again, it should have a new exit data to return
        // if for some strange reason it does not, then the loop will run again
        exitData = exitDataDequeue(currpcb->m_edQ);
    }

    // success
    TracePrintf(2, "Process successfully got a child's exit data.\n");
    *status_ptr = exitData->m_status;
    int pid = exitData->m_pid;
    free(exitData);
    return pid;

    // temp code end
    /*
    else if (exitData == NULL)
    {
        // no exited children but running children, so move it to gWaitProcessQ
        processEnqueue(&gWaitProcessQ, currPCB);
        processDequeue(&gRunningProcessQ);
        // TODO, context switch here instead

        // when this process picks up again, it should have a new exit data to returns
        exitData = exitDataDequeue(currPCB->m_edQ);
        *status_ptr = -1;
        return ERROR;   // not sure what to return in this case
    }
    else
    {
        // success
        *status_ptr = exitData->m_status;
        return exitData->m_pid;
    }
    */
}

int kernelGetPid(void) {
	// Find the pid of the calling process and return it
	PCB* currPCB = getHeadProcess(&gRunningProcessQ);
	return currPCB->m_pid;
}

// Brk raises or lowers the value of the process's brk to contain addr
// NOTE: Check if brk enters into data-text segments.
//       return error in any case.!
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
    PCB* currpcb = getHeadProcess(&gRunningProcessQ);
    UserProgPageTable* currpt = currpcb->m_pagetable;
    unsigned int currBrk = currpcb->m_brk;
    unsigned int brkPgNum = currBrk / PAGESIZE;

    // reset brkpg num to offsetted page within r1
    brkPgNum -= gNumPagesR0;
    TracePrintf(2, "The current brk page is: %d\n", brkPgNum);
    unsigned int delta;
    unsigned int pgDiff;
    int i;

    if(newAddr >= currBrk)
    {
        // allocate memory
        delta = (newAddr - currBrk);
        pgDiff = delta / PAGESIZE;

        /// TODO: error handling if we run out of free frames
        for(i = 1; i <= pgDiff; i++)
        {
            FrameTableEntry* frame = getOneFreeFrame(&gFreeFramePool, &gUsedFramePool);
            if(frame != NULL)
            {
                unsigned int pfn = frame->m_frameNumber;
                currpt->m_pte[brkPgNum+i].valid = 1; currpt->m_pte[brkPgNum+i].prot = PROT_READ | PROT_WRITE; currpt->m_pte[brkPgNum+i].pfn = pfn;
                TracePrintf(2, "The allocated page number is %d and the frame number is %d\n", brkPgNum+i, pfn);
            }
            else
            {
                TracePrintf(0, "Could not find free frames\n");
                return ERROR;
            }
        }
    }
    else
    {
        // deallocate memory
        delta = (currBrk - newAddr);
        pgDiff = delta / PAGESIZE;
        for(i = 0; i < pgDiff; i++)
        {
            currpt->m_pte[brkPgNum - i].valid = 0;
            currpt->m_pte[brkPgNum - i].prot = PROT_NONE;
            int pfn = currpt->m_pte[brkPgNum - i].pfn;
            freeOneFrame(&gFreeFramePool, &gUsedFramePool, pfn);
            TracePrintf(2, "The freed page number is %d and the frame number is %d\n", brkPgNum-i, pfn);
        }
    }

    TracePrintf(2, "The page difference is: %d\n", pgDiff);

    // set the brk to be the new address
    currpcb->m_brk = newAddr;

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
        processDequeue(&gRunningProcessQ);
        processEnqueue(&gSleepBlockedQ, currPCB);
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
    return SUCCESS;
}

// TTYWrite writes to the terminal tty_id
// returns the number of bytes it wrote to the terminal
// the only way to return an error is by returning a value that is not equal to the len
// we use -1 in this case
int kernelTtyWrite(int tty_id, void *buf, int len)
{
    // Add a request to the terminal queue
    PCB* currpcb = getHeadProcess(&gRunningProcessQ);
    int written = 0;

    // Each process creates a request
    // create a request and then goes to sleep
    // when the TRAP_WRITE is received, that process then goes to sleep
    // picks the first process in the queue allowing it to finish
        // the process wakes here
        // if its done with its work, it puts itself in running queue
        // removes itself from the blocked queue
        // continues out of this call

    TerminalRequest* head = &gTermWReqHeads[tty_id];

    // find the first empty slot
    TerminalRequest* curr = head;
    TerminalRequest* next = curr->m_next;
    while(next != NULL)
    {
        curr = next;
        next = curr->m_next;
    }

    // create the new entry for this request
    TerminalRequest* req = (TerminalRequest*)malloc(sizeof(TerminalRequest));
    if(req != NULL)
    {
        req->m_code = TERM_REQ_WRITE;
        req->m_pcb = currpcb;
        req->m_bufferR0 = (void*)malloc(sizeof(char) * len);
        req->m_next = NULL;
        if(req->m_bufferR0 != NULL)
        {
            // append the request to the queue of request
            curr->m_next = req;
            memcpy(req->m_bufferR0, buf, sizeof(char) * len);
            req->m_len = len;
            req->m_serviced = 0;
            req->m_remaining = len;
            req->m_next = NULL;

            int iter = len / TERMINAL_MAX_LINE;
            if(len % TERMINAL_MAX_LINE != 0) iter += 1;     // add one more iter if we dont have perfect sizes
            int i;
            for(i = 0; i < iter; i++)
            {
                int toSend = req->m_remaining;
                toSend = toSend > TERMINAL_MAX_LINE ? TERMINAL_MAX_LINE : toSend;
                TtyTransmit(tty_id, req->m_bufferR0 + req->m_serviced, toSend);

                // context switch
                processRemove(&gRunningProcessQ, currpcb);
                processEnqueue(&gWriteBlockedQ, currpcb);
                PCB* nextpcb = getHeadProcess(&gReadyToRunProcessQ);
                int rc = KernelContextSwitch(SwitchKCS, currpcb, nextpcb);
                if(rc == -1)
                {
                    TracePrintf(0, "Context switch failed in terminal write. Returning without writing\n");

                    // remove this node from gTermWReqHeads queue
                    if(removeTerminalRequest(tty_id, req) != 0)
                    {
                        TracePrintf(1, "ERROR: Removing the request failed\n");
                    }
                    processRemove(&gWriteBlockedQ, currpcb);
                    processEnqueue(&gRunningProcessQ, currpcb);
                    swapPageTable(currpcb);
                    return -1;
                }

                // we wake up after we have written successfully to terminal
                processRemove(&gWriteBlockedQ, currpcb);
                processEnqueue(&gRunningProcessQ, currpcb);
                swapPageTable(currpcb);
                req->m_serviced += toSend;
            }
        }
        else
        {
            TracePrintf(0, "Error could not allocate memory for storing the amount %d bytes within the request\n", len);
            free(req);
            return -1;
        }
    }
    else
    {
        TracePrintf(0, "Error: Couldnt allocate memory for terminal request");
        return -1;
    }

    // remove the request from the queue and associated Memory
    int serviced = req->m_serviced;
    if(removeTerminalRequest(tty_id, req) != 0)
        TracePrintf(1, "ERROR: Removing the request failed\n");

    // return the amount that was serviced
    return serviced;
}

int kernelPipeInit(int *pipe_idp)
{
	// Create a new pipe with a unique id, owned by the calling process
    // Save the id into pipe_idp
    int uid = getUniqueSyncId(SYNC_PIPE);
    if(uid == 0xFFFFFFFF)
        return ERROR;
    else
    {
        pipeEnqueue(uid);
        *pipe_idp = uid;
        return SUCCESS;
    }
}

int kernelPipeRead(int pipe_id, void *buf, int len, int* actuallyRead)
{
    PCB* currpcb = getHeadProcess(&gRunningProcessQ);
    PipeQueueNode* pipeNode = getPipeNode(pipe_id);
    if(pipeNode == NULL)
    {
        TracePrintf(0, "ERROR: Invalid pipe id provided\n");
        return ERROR;
    }
    else
    {
        Pipe* p = pipeNode->m_pipe;
        if(len <= p->m_validLength)
        {
            // request served immediately
            memcpy(buf, p->m_buffer, sizeof(char) * len);
            *actuallyRead = len;
            return len;
        }
        else
        {
            // we dont have enough bytes
            *actuallyRead = 0;
            return 0;
        }
    }
}

int kernelPipeWrite(int pipe_id, void *buf, int len)
{
	PipeQueueNode* pipeNode = getPipeNode(pipe_id);
    if(pipeNode == NULL)
    {
        TracePrintf(0, "ERROR: Invalid pipe id provided\n");
        return ERROR;
    }
    else
    {
        Pipe* p = pipeNode->m_pipe;
        if(p->m_validLength == 0)
        {
            // the first time this is called
            // so allocate memory
            void* pdata = (void*)malloc(sizeof(char) * len);
            if(pdata != NULL)
            {
                p->m_len = len;
                p->m_validLength = len;
                p->m_buffer = pdata;
                memcpy(p->m_buffer, buf, sizeof(char) * len);
                return len;
            }
            else
            {
                TracePrintf(0, "ERROR: Unable to allocate memroy for pipe\n");
                return ERROR;
            }
        }
        else
        {
            // check if the new size is greater than old size
            // if so, free the current memory
            // and allocate new memory for pipe
            // else if less, clear the memory, and modify the valid length
            if(len > p->m_len)
            {
                free(p->m_buffer);
                p->m_buffer = (void*)malloc(sizeof(char) * len);
                p->m_len = len;
                p->m_validLength = len;
                memcpy(p->m_buffer, buf, sizeof(char) * len);
                return len;
            }
            else
            {
                memset(p->m_buffer, 0, sizeof(char) * p->m_len);
                memcpy(p->m_buffer, buf, sizeof(char) * len);
                p->m_validLength = len;
                return len;
            }
        }
    }
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

int kernelAcquire(int lock_id, UserContext* ctx)
{
    PCB* currpcb = getHeadProcess(&gRunningProcessQ);

    LockQueueNode* lockNode = getLockNode(lock_id);
    if(lockNode == NULL)
    {
        return ERROR;
    }
    else if(lockNode->m_holder == currpcb->m_pid)
    {
        // calling process already holds the lock
        return ERROR;
    }

    Lock* lock = lockNode->m_lock;
    if(lock->m_state == UNLOCKED)
    {
        // if the lock is free, update the lock's holder and continue running
        lockNode->m_holder = currpcb->m_pid;
        lock->m_state = LOCKED;
    }
    else
    {
        // Else, add the calling process to the lock referenced by lock_id's queue of waiting processes
        processDequeue(&gRunningProcessQ);
        lockWaitingEnqueue(lockNode, currpcb);
    }

    if(getHeadProcess(&gRunningProcessQ) == NULL)
    {
        // calling process is now waiting on a lock, so context switch here
        PCB* nextpcb = getHeadProcess(&gReadyToRunProcessQ);
        memcpy(currpcb->m_uctx, ctx, sizeof(UserContext));
        int rc = KernelContextSwitch(SwitchKCS, currpcb, nextpcb);
        if(rc == -1)
        {
            TracePrintf(0, "Kernel Context switch failed\n");
            exit(-1);
        }
        processRemove(&gReadyToRunProcessQ, currpcb);
        processEnqueue(&gRunningProcessQ, currpcb);
        currpcb->m_ticks = 0;

        // swap out the page tables
        swapPageTable(currpcb);
        memcpy(ctx, currpcb->m_uctx, sizeof(UserContext));
    }

    return SUCCESS;
}

int kernelRelease(int lock_id) {
    PCB* currPCB = getHeadProcess(&gRunningProcessQ);

    LockQueueNode* lockNode = getLockNode(lock_id);
    if(lockNode == NULL)
    {
        // if the lock doesn't exist, return ERROR
        return ERROR;
    }
    Lock* lock = lockNode->m_lock;

    if(lockNode->m_holder != currPCB->m_pid)
    {
        // If the caller doesn't own the lock, return an error
        return ERROR;
    }
    else
    {
        // Otherwise unlock the lock and give it to the next waiting process if there is one
        lock->m_state = UNLOCKED;
        lockNode->m_holder = -1;
        PCB* newLockHolder = lockWaitingDequeue(lockNode);
        if(newLockHolder != NULL)
        {
            lockNode->m_holder = newLockHolder->m_pid;
            lock->m_state = LOCKED;
            processEnqueue(&gReadyToRunProcessQ, newLockHolder);
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
    CVarQueueNode* cvarNode = getCVarNode(cvar_id);
    if(cvarNode == NULL)
    {
        // no cvar was initialized
        return ERROR;
    }

    // Wake up a waiter
    PCB* unblockedPCB = processDequeue(cvarNode->m_waitingQueue);
    if(unblockedPCB != NULL)
    {
        processEnqueue(&gReadyToRunProcessQ, unblockedPCB);
    }
    return SUCCESS;
}

int kernelCvarBroadcast(int cvar_id) {
	// Find the cvar referred to by cvar_id and notify all processes waiting on it
    CVarQueueNode* cvarNode = getCVarNode(cvar_id);
    if(cvarNode == NULL)
    {
        // no cvar was initialized
        return ERROR;
    }

    PCB* unblockedpcb = processDequeue(cvarNode->m_waitingQueue);
    while(unblockedpcb != NULL)
    {
        processEnqueue(&gReadyToRunProcessQ, unblockedpcb);
        unblockedpcb = processDequeue(cvarNode->m_waitingQueue);
    }

    return SUCCESS;
}

int kernelCvarWait(int cvar_id, int lock_id, UserContext* ctx)
{
    PCB* currpcb = getHeadProcess(&gRunningProcessQ);
    CVarQueueNode* cvarNode = getCVarNode(cvar_id);
    LockQueueNode* lockNode = getLockNode(lock_id);
    if(cvarNode == NULL || lockNode == NULL)
    {
        // no cvar or lock were initialized
        return ERROR;
    }

    // Throw error is the calling process doesn't currently hold the lock
    if(lockNode->m_holder != currpcb->m_pid)
    {
        return ERROR;
    }

    // update the cvars lock id on the first time ONLY, else throw an ERROR
    if(cvarNode->m_cvar->m_lockId == -1)
    {
        cvarNode->m_cvar->m_lockId = lock_id;
    }
    else if(cvarNode->m_cvar->m_lockId != lock_id)
    {
        return ERROR;
    }

    // Release the lock referenced by lock_id
    int release_rc = kernelRelease(lock_id);
    if(release_rc == ERROR)
    {
        return ERROR;
    }

    // Add the calling process to the waiting queue for cvar_id
    processDequeue(&gRunningProcessQ);
    cvarWaitingEnqueue(cvarNode, currpcb);

    // ****** scheduler *******
    // run another process while waiting to be notified by a CvarSignal or CvarBroadcast
    PCB* nextpcb = getHeadProcess(&gReadyToRunProcessQ);
    memcpy(currpcb->m_uctx, ctx, sizeof(UserContext));
    int rc = KernelContextSwitch(SwitchKCS, currpcb, nextpcb);
    if(rc == -1)
    {
        TracePrintf(0, "Kernel Context switch failed\n");
        exit(-1);
    }
    processRemove(&gReadyToRunProcessQ, currpcb);
    processEnqueue(&gRunningProcessQ, currpcb);
    currpcb->m_ticks = 0;

    // swap out the page tables
    swapPageTable(currpcb);
    memcpy(ctx, currpcb->m_uctx, sizeof(UserContext));
    // ****** scheduler *******

    // Acquire the lock again
    rc = kernelAcquire(lock_id, ctx);
    if(rc == ERROR)
    {
        return ERROR;
    }
    return SUCCESS;
}

int kernelReclaim(int id) {
	// If the calling process is not the owner of the lock/cvar/pipe referenced by id, return ERROR
	// Free all resources held by the lock/cvar/pipe (usually nodes and waiting queues)
    // Remove the lock/cvar/pipe from its global list
    SyncType t = getSyncType(id);
    if(t == SYNC_LOCK)
    {
        LockQueueNode* lockNode = getLockNode(id);
        if(lockNode == NULL)
        {
            TracePrintf(0, "ERROR: Invalid syscall to free a non-existent lock\n");
            return ERROR;
        }
        else
        {
            return freeLock(lockNode);
        }
    }
    else if(t == SYNC_CVAR)
    {
        CVarQueueNode* cvarNode = getCVarNode(id);
        if(cvarNode == NULL)
        {
            TracePrintf(0, "ERROR: Invalid syscall to free a non-existent cvar\n");
            return ERROR;
        }
        else
        {
            return freeCVar(cvarNode);
        }
    }
    else if(t == SYNC_PIPE)
    {
        PipeQueueNode* pipeNode = getPipeNode(id);
        if(pipeNode == NULL)
        {
            TracePrintf(0, "ERROR: Invalid syscall to free a non-existent pipe\n");
            return ERROR;
        }
        else
        {
            return freePipe(pipeNode);
        }
    }
    else
    {
        return ERROR;
    }
}
