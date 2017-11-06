#include <interrupt_handler.h>
#include <process.h>
#include <scheduler.h>
#include <syscalls.h>
#include <terminal.h>
#include <yalnix.h>
#include <yalnixutils.h>

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
					//processEnqueue(&gRunningProcessQ, processDequeue(&gWaitProcessQ));
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
			{
				PCB* currpcb = getHeadProcess(&gRunningProcessQ);
				memcpy(currpcb->m_uctx, ctx, sizeof(UserContext));

				if(currpcb->m_kctx == NULL)
				{
					// do a dummy context switch to get the current kernel context
					KernelContextSwitch(MyKCS, currpcb, NULL);
				}

				int tty_id = ctx->regs[0];
				void* buf = (void*)(ctx->regs[1]);
				int len = ctx->regs[2];
				kernelTtyRead(tty_id, buf, len);

				// do a context switch
				PCB* nextpcb = processDequeue(&gReadyToRunProcessQ);
				currpcb->m_ticks = 0;
				nextpcb->m_ticks = 0;
				if(nextpcb != NULL)
				{
					int rc = KernelContextSwitch(MyKCS, currpcb, nextpcb);
					if(rc == -1)
					{
						TracePrintf(0, "Context switch failed within tty read\n");
					}
					else
					{
						processRemove(&gRunningProcessQ, currpcb);
						processEnqueue(&gReadBlockedQ, currpcb);
						processEnqueue(&gRunningProcessQ, nextpcb);
						swapPageTable(nextpcb);
						memcpy(ctx, nextpcb->m_uctx, sizeof(UserContext));
						return;
					}
				}
				else
				{
					TracePrintf(0, "No free process to run\n");
				}
			}
			break;
		case YALNIX_TTY_WRITE:
			{
				PCB* currpcb = getHeadProcess(&gRunningProcessQ);
				memcpy(currpcb->m_uctx, ctx, sizeof(UserContext));

				if(currpcb->m_kctx == NULL)
				{
					// do a dummy context switch to get the current kernel context
					KernelContextSwitch(MyKCS, currpcb, NULL);
				}

				int tty_id = ctx->regs[0];
				void* buf = (void*)(ctx->regs[1]);
				int len = ctx->regs[2];
				kernelTtyWrite(tty_id, buf, len);

				// initiate the first transfer and wait for hardware to trap
				if(gTermWReqHeads[tty_id].m_requestInitiated == 0)
				{
					// initiate the handling of a transfer.
					// the future remaining requests are handled by the trap handler
					// when the terminal fires up the interrupts.
					processOutstandingWriteRequests(ctx->regs[0]);
					gTermWReqHeads[tty_id].m_requestInitiated = 1;
				}

				// do a context switch here
				PCB* nextpcb = processDequeue(&gReadyToRunProcessQ);
				currpcb->m_ticks = 0;
				nextpcb->m_ticks = 0;
				if(nextpcb != NULL)
				{
					int rc = KernelContextSwitch(MyKCS, nextpcb, currpcb);
					if(rc == -1)
					{
						TracePrintf(0, "Context switch failed");
					}

					processRemove(&gRunningProcessQ, currpcb);
					processEnqueue(&gWriteBlockedQ, currpcb);
					processEnqueue(&gRunningProcessQ, nextpcb);
					swapPageTable(nextpcb);
					memcpy(ctx, nextpcb->m_uctx, sizeof(UserContext));
					return;
				}
				else
				{
					TracePrintf(0, "Error: No process to run.!!\n");
				}
			}
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

	// check if any process that was in the read wait queue and we have data to service it
	if(gReadFinishedQ.m_head != NULL)
	{
		PCB* process = processDequeue(&gReadFinishedQ);
		while(process != NULL)
		{
			processEnqueue(&gReadyToRunProcessQ, process);
			process = processDequeue(&gReadFinishedQ);
		}
	}

	if(gWriteFinishedQ.m_head != NULL)
	{
		PCB* process = processDequeue(&gWriteFinishedQ);
		while(process != NULL)
		{
			processEnqueue(&gReadyToRunProcessQ, process);
			process = processDequeue(&gWriteFinishedQ);
		}
	}

	// if this process has run for too long
	// say 3 ticks, then swap it with a different process in the ready to run queue
	if(currPCB->m_ticks > 1)
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
			if(nextPCB->m_iodata != NULL)
			{
				// we have io data ready for this process
				TerminalRequest* iodata = (TerminalRequest*)nextPCB->m_iodata;
				memcpy(iodata->m_bufferR1, iodata->m_bufferR0, iodata->m_remaining);
				ctx->regs[0] = iodata->m_remaining;

				// delete the data that was used for io
				free(iodata->m_bufferR0);
				free(iodata);
			}
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
	PCB* currpcb = getHeadProcess(&gRunningProcessQ);
	TracePrintf(0, "Illegal Interrupt Happened.\nKilling Process : %d\n", currpcb->m_pid);
	// move the process to dead queue
	// inform any waiting parent of this processes state.
}

// Interrupt handler for memory
void interruptMemory(UserContext* ctx)
{
	// Get the current running processes pcb
	PCB* currPCB = getHeadProcess(&gRunningProcessQ);
	memcpy(currPCB->m_uctx, ctx, sizeof(UserContext));

	// get the code for the memory interrupt.
	int code = ctx->code;
	if(code == YALNIX_ACCERR)
	{
		TracePrintf(0, "Memory trap for a page with invalid access permissions\n");
		// TODO: How to handle this???
		return;
	}

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
	int tty_id = ctx->code;
	processOutstandingReadRequests(tty_id);
}

// Interrupt Handler for terminal transmit
void interruptTtyTransmit(UserContext* ctx)
{
	// check for correctness of data from transmit message from terminal
	// move the data from terminal's address space to kernel's address space
	// move the data to any process that is waiting on terminal data/
	// in case of a valid command for process
		// call fork() - exec with the parameters
	int tty_id = ctx->code;
	processOutstandingWriteRequests(tty_id);
}

// This is a dummy interrupt handler that does nothing.
// for the rest of the IVT entries
void interruptDummy(UserContext* ctx)
{

}
