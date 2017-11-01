#include <interrupt_handler.h>
#include <process.h>
#include <syscalls.h>
#include <yalnix.h>
#include <yalnixutils.h>
#include <scheduler.h>

int debug = 1;

extern KernelContext* MyKCS(KernelContext* kc_in, void* curr_pcb_p, void* next_pcb_p);

// Interrupt handler for kernel syscalls
void interruptKernel(UserContext* ctx)
{
	// get code for the kernel system call
	int code = ctx->code;
	switch(code)
	{
        // call appropriate system call with the user context structure passed
		case YALNIX_FORK:
			{
				// the return codes are stored in the pcb's user context
				// update the child's kernel context
				PCB* parentpcb = getHeadProcess(&gRunningProcessQ);
				memcpy(parentpcb->m_uctx, ctx, sizeof(UserContext));
				int rc = kernelFork();
				if(rc != SUCCESS)
				{
					TracePrintf(0, "Fork() failed\n");
				}
				else
				{
					TracePrintf(0, "Fork() success\n");
					if(parentpcb->m_kctx == NULL)
					{
						// perform a dummy context switch here to give the child
						// the starting location to run from
						rc = KernelContextSwitch(MyKCS, parentpcb, NULL);
						if(rc == -1)
						{
							TracePrintf(0, "Error obtaining kernel context during the fork\n");
						}
						else
						{
							PCB* childpcb = getPcbByPid(&gReadyToRunProcessQ, parentpcb->m_uctx->regs[0]);
							if(childpcb != NULL && childpcb->m_kctx != NULL)
							{
								memcpy(childpcb->m_kctx, parentpcb->m_kctx, sizeof(KernelContext));
							}
							else
							{
								TracePrintf(0, "Did not find child process\n");
							}
						}
					}
				}
			}
			break;
		case YALNIX_EXEC:
			{
				// the return codes are stored in the pcb's user context
				// update the child's kernel context
				PCB* currpcb = getHeadProcess(&gRunningProcessQ);
				memcpy(currpcb->m_uctx, ctx, sizeof(UserContext));
				char* filename = (char*)(ctx->regs[0]);
				char** argvec = (char**)(ctx->regs[1]);
				TracePrintf(0, "Exec arg0 : %s\n", filename);
				TracePrintf(0, "Exec argv[0]: %s\n", argvec[0]);
				int rc = kernelExec(filename, argvec);
				if(rc != SUCCESS)
				{
					TracePrintf(0, "Exec failed\n");
				}
				else
				{
					// Start executing from the pc and sp pointers that
					// were filled in the Exec system call.
					memcpy(ctx, currpcb->m_uctx, sizeof(UserContext));
				}
			}
			break;
		case YALNIX_EXIT:
			{
				int status = 42;	// figure out how to get the real status
				kernelExit(status);
			}
			break;
        case YALNIX_WAIT:
            {
                // try to find some other process to run in its place
				int status;
				int child_pid = kernelWait(&status);
				if(getHeadProcess(&gRunningProcessQ) == NULL)
				{
					// waiting happened, so we context switch
					TracePrintf(2, "Process waiting for a child to exit\n");
					// For now, just add an ed and restart init
					ExitData* ed = (ExitData*)malloc(sizeof(ExitData));
					if(ed == NULL)
					{
						TracePrintf(0, "Failed to malloc for exit data\n");
						exit(-1);
					}
					ed->m_status = 6;
					ed->m_pid = 66;
					processEnqueue(&gRunningProcessQ, processDequeue(&gWaitProcessQ));
					exitDataEnqueue(getHeadProcess(&gRunningProcessQ)->m_edQ, ed);
				}
				else
				{
					TracePrintf(2, "Process %d exited w/ status %d so I don't have to wait.\n", child_pid, status);
				}
            }
        	break;
        case YALNIX_BRK:
            {
				void* addr = (void *)ctx->regs[0];
				TracePrintf(2, "Brk address is: %x\n", addr);
            	kernelBrk(addr);
            }
            break;
        case YALNIX_GETPID:
            {
				int pid = kernelGetPid();
				ctx->regs[0] = (u_long)pid;
            }
            break;
		case YALNIX_DELAY:
				{
					int clock_ticks = ctx->regs[0];
					int rc = kernelDelay(clock_ticks);
					if(rc == SUCCESS)
					{
						TracePrintf(2, "Delay was successful\n");

						if(getHeadProcess(&gRunningProcessQ) == NULL)
						{
							// delay happened, so we can context switch
							PCB* currPCB = getHeadProcess(&gSleepBlockedQ);
							PCB* nextPCB = getHeadProcess(&gReadyToRunProcessQ);
							memcpy(currPCB->m_uctx, ctx, sizeof(UserContext));
							int rc = KernelContextSwitch(MyKCS, currPCB, nextPCB);
							if(rc == -1)
							{
								TracePrintf(0, "Kernel Context switch failed\n");
								exit(-1);
							}

							processEnqueue(&gRunningProcessQ, nextPCB);
							processDequeue(&gReadyToRunProcessQ);

							// swap out the page tables
							swapPageTable(nextPCB);

							memcpy(ctx, nextPCB->m_uctx, sizeof(UserContext));
							return;
							}
						}
					}
				break;
        	default:
            // all others are not implemented syscalls are not implemented.
            	break;
		case YALNIX_TTY_READ:
			break;
		case YALNIX_TTY_WRITE:
			break;
	}
}

// Interrupt handler for clock
// Logic: The OS will invoke the scheduler to swap existing processes after the have obtained the
//  	  enough quanta of runtime, checks for processes waiting on IO and moves them to the correct global queues as necessary.
void interruptClock(UserContext* ctx)
{
	// Handle movement of processes from different waiting/running/exited queues
	// Handle the cleanup of potential swapped out pages
	TracePrintf(3, "TRAP_CLOCK\n");

	// update the quantum of runtime for the current running process
	PCB* currPCB = getHeadProcess(&gRunningProcessQ);
  	if(currPCB->m_kctx == NULL)
	{
		// get the first kernel context
		int rc = KernelContextSwitch(MyKCS, currPCB, NULL);
		if(rc == -1)
		{
			TracePrintf(0, "Getting first context failed");
			exit(-1);
		}
	}
	currPCB->m_ticks++;

	/* Decrement the sleep time of each PCB in the sleep queue
	 * If the PCB's sleep time is zero, it will be moved to the ready to run queue
	*/
	scheduleSleepingProcesses();

	/* Exit logic?
	*/

	// if this process has run for too long
	// say 3 ticks, then swap it with a different process in the ready to run queue
	if(currPCB->m_ticks > 2)
	{
		// schedule logic
		if(getHeadProcess(&gReadyToRunProcessQ) != NULL)
		{
			memcpy(currPCB->m_uctx, ctx, sizeof(UserContext));
			TracePrintf(0, "We have a process to schedule out\n");

			// update the user context
			currPCB->m_ticks = 0;	// reset ticks
			PCB* nextPCB = getHeadProcess(&gReadyToRunProcessQ);
			int rc = KernelContextSwitch(MyKCS, currPCB, nextPCB);
			if(rc == -1)
			{
				TracePrintf(0, "Kernel Context switch failed\n");
				exit(-1);
			}

			// swap the two pcbs
			processDequeue(&gRunningProcessQ);
			processEnqueue(&gRunningProcessQ, nextPCB);
			processDequeue(&gReadyToRunProcessQ);					// remove the process that was picked to run
			processEnqueue(&gReadyToRunProcessQ, currPCB);

			// swap out the page tables
			swapPageTable(nextPCB);

			memcpy(ctx, nextPCB->m_uctx, sizeof(UserContext));
			return;
		}
		else if(currPCB->m_kctx == NULL)
		{
			// THIS BRANCH IS PROBABLY A DEAD BRANCH
			// get the current kernel context to have the context available for successful context switch
			KernelContextSwitch(MyKCS, currPCB, NULL);
		}
	}
}

// Interrupt handler for illegal instruction
void interruptIllegal(UserContext* ctx)
{
	// display the error to log
	// kill the process
		// move the process from running to dead queue
}

// Interrupt handler for memory
void interruptMemory(UserContext* ctx)
{
	// Get the current running processes pcb
	PCB* currPCB = getHeadProcess(&gRunningProcessQ);

	// compute the locations of the page from the top of the VM
	// and also get the location of the brk of the process
	unsigned int loc = (unsigned int)ctx->addr;
	unsigned int brkloc = (unsigned int)currPCB->m_brk;
	unsigned int pg = (loc) / PAGESIZE;
	unsigned int brkpg = (brkloc) / PAGESIZE;
	if(currPCB->m_pt->m_pte[pg].valid == 0)
	{
		// check to make sure that we are not silently growing into the heap.!!
		if(pg - brkpg > 2)
		{
			// we might be actually requesting for a large chunk of frames
			// and not just a single frame. Hence we start from the top of the VM region
			// move down the page table, and once we start encountering the invalid pages, we
			// allocate frames till we reach the page where the trap was invoked.
			// this might seem wasteful, but we have no clear way to store the current
			// stack page.
			// TODO: Add a PCB entry for the stack region of each process in R1.

			unsigned int r1Top = VMEM_1_LIMIT / PAGESIZE;
			int currpg;
			for(currpg = r1Top; currpg >= pg; currpg--)
			{
				if(currPCB->m_pt->m_pte[currpg].valid == 1) continue;
				else
				{
					// started finding the invalid pages.
					FrameTableEntry* frame = getOneFreeFrame(&gFreeFramePool, &gUsedFramePool);
					if(frame != NULL)
					{
						currPCB->m_pt->m_pte[pg].valid = 1;
						currPCB->m_pt->m_pte[pg].prot = PROT_READ | PROT_WRITE;
						currPCB->m_pt->m_pte[pg].pfn = frame->m_frameNumber;
					}
					else
					{
						TracePrintf(0, "Could not find one free page\n");
					}
				}
			}
		}
		else
		{
			TracePrintf(0, "User program stack region exhausted and might grow into heap.!\n");
			TracePrintf(0, "Not allocating the requested page\n");
			// TODO: kill the process and move it to terminated queue.
		}
	}
	else
	{
		TracePrintf(0, "Interrupt memory in a page that is valid? Something fishy!\n");
	}
}

// Interrupt handler for math traps
void interruptMath(UserContext* ctx)
{
	// check the status of the register for what caused this math traps
	// report to user and determine further course of action
}

// Interrupt Handler for terminal recieve
void interruptTtyReceive(UserContext* ctx)
{
	// allocate memory in kernel space for moving contents from
	// terminal memory to kernel memory.
	// move the data from kernel memory into process space of any process waiting on data from the terminal
	// free up the data from kernel memory.
}

// Interrupt Handler for terminal transmit
void interruptTtyTransmit(UserContext* ctx)
{
	// check for correctness of data from transmit message from terminal
	// move the data from terminal's address space to kernel's address space
	// move the data to any process that is waiting on terminal data/
	// in case of a valid command for process
		// call fork() - exec with the parameters
}

// This is a dummy interrupt handler that does nothing.
// for the rest of the IVT entries
void interruptDummy(UserContext* ctx)
{

}
