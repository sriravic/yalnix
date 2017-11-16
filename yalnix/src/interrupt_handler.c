#include <interrupt_handler.h>
#include <process.h>
#include <scheduler.h>
#include <syscalls.h>
#include <terminal.h>
#include <yalnix.h>
#include <yalnixutils.h>

int debug = 1;

extern KernelContext* GetKCS(KernelContext* kc_in, void* curr_pcb_p, void* next_pcb_p);
extern KernelContext* SwitchKCS(KernelContext* kc_in, void* curr_pcb_p, void* next_pcb_p);

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
				PCB* currpcb = getHeadProcess(&gRunningProcessQ);
				memcpy(currpcb->m_uctx, ctx, sizeof(UserContext));
				int rc = kernelFork();
				if(rc != SUCCESS)
				{
					TracePrintf(0, "Fork() failed\n");
					currpcb->m_uctx->regs[0] = -1;
					memcpy(ctx, currpcb->m_uctx, sizeof(UserContext));
					return;
				}
				else
				{
					PCB* torun = getHeadProcess(&gRunningProcessQ);
					memcpy(ctx, torun->m_uctx, sizeof(UserContext));
					return;
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
				TracePrintf(2, "Process exited\n");
				int status = ctx->regs[0];	// figure out how to get the real status
				kernelExit(status, ctx);
			}
		break;
        case YALNIX_WAIT:
            {
                // try to find some other process to run in its place
				int *status_ptr = (int*)ctx->regs[0];
				ctx->regs[0] = kernelWait(status_ptr, ctx);
            }
        break;
        case YALNIX_BRK:
            {
				void* addr = (void *)ctx->regs[0];
				TracePrintf(3, "INFO : Brk address is: %x\n", addr);
            	int rc = kernelBrk(addr);
				ctx->regs[0] = rc;
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
				ctx->regs[0] = kernelDelay(clock_ticks, ctx);
			}
		break;
		case YALNIX_TTY_READ:
			{
				PCB* currpcb = getHeadProcess(&gRunningProcessQ);
				memcpy(currpcb->m_uctx, ctx, sizeof(UserContext));
				int tty_id = ctx->regs[0];
				void* buf = (void*)(ctx->regs[1]);
				int len = ctx->regs[2];

				// Perform some checks
				int allokay = 0;
				if(tty_id >= NUM_TERMINALS) { allokay = 1; TracePrintf(0, "ERROR: Invalid terminal number\n"); }
				if(checkValidAddress((unsigned int)buf, currpcb) != 0) { allokay = 2; TracePrintf(0, "ERROR: Invalid address\n"); }
				if(len < 0) { allokay = 3; TracePrintf(0, "ERROR: Invalid Length specified for write\n"); }

				if(allokay != 0)
				{
					// We encountered an error.!
					// Syscall Specs: Return ERROR
					currpcb->m_uctx->regs[0] = ERROR;
					memcpy(ctx, currpcb->m_uctx, sizeof(UserContext));
					return;
				}
				else
				{
					// The current process will go inside and do a context switch
					// it will be repeatedly context switched till it either completes correctly
					// or fails. It will come out of this function and continue executing into userland
					int read = kernelTtyRead(tty_id, buf, len);
					if(read == len)
						currpcb->m_uctx->regs[0] = len;
					else currpcb->m_uctx->regs[0] = ERROR;
					memcpy(ctx, currpcb->m_uctx, sizeof(UserContext));
					return;
				}
			}
		break;
		case YALNIX_TTY_WRITE:
			{
				PCB* currpcb = getHeadProcess(&gRunningProcessQ);
				memcpy(currpcb->m_uctx, ctx, sizeof(UserContext));
				int tty_id = ctx->regs[0];
				void* buf = (void*)(ctx->regs[1]);
				int len = ctx->regs[2];

				// Perform some error checks
				int allokay = 0;
				if(tty_id >= NUM_TERMINALS) { allokay = 1; TracePrintf(0, "ERROR: Invalid terminal number\n"); }
				if(checkValidAddress((unsigned int)buf, currpcb) != 0) { allokay = 2; TracePrintf(0, "ERROR: Invalid address\n"); }
				if(len < 0) { allokay = 3; TracePrintf(0, "ERROR: Invalid Length specified for write\n"); }

				if(allokay != 0)
				{
					// We encountered an error.!
					// Syscall Specs: Return ERROR
					currpcb->m_uctx->regs[0] = ERROR;
					memcpy(ctx, currpcb->m_uctx, sizeof(UserContext));
					return;
				}
				else
				{
					// The current process will go inside and do a context switch
					// it will be repeatedly context switched till it either completes correctly
					// or fails. It will come out of this function and continue executing into userland
					int written = kernelTtyWrite(tty_id, buf, len);
					if(written == len)
						currpcb->m_uctx->regs[0] = len;
					else currpcb->m_uctx->regs[0] = ERROR;
					memcpy(ctx, currpcb->m_uctx, sizeof(UserContext));
					return;
				}
			}
		break;
		case YALNIX_LOCK_INIT:
			{
				int* lock_idp = (int*)ctx->regs[0];
				ctx->regs[0] = kernelLockInit(lock_idp);
			}
		break;
		case YALNIX_LOCK_ACQUIRE:
			{
				PCB* currpcb = getHeadProcess(&gRunningProcessQ);
				int lock_id = ctx->regs[0];
				ctx->regs[0] = kernelAcquire(lock_id, ctx);
			}
		break;
		case YALNIX_LOCK_RELEASE:
			{
				int lock_id = ctx->regs[0];
				ctx->regs[0] = kernelRelease(lock_id);
			}
		break;
		case YALNIX_CVAR_INIT:
			{
				int *cvar_idp = (int*)ctx->regs[0];
				ctx->regs[0] = kernelCvarInit(cvar_idp);
			}
		break;
		case YALNIX_CVAR_SIGNAL:
			{
				int cvar_id = ctx->regs[0];
				ctx->regs[0] = kernelCvarSignal(cvar_id);
			}
		break;
		case YALNIX_CVAR_BROADCAST:
			{
				int cvar_id = ctx->regs[0];
				ctx->regs[0] = kernelCvarBroadcast(cvar_id);
			}
		break;
		case YALNIX_CVAR_WAIT:
			{
				int cvar_id = ctx->regs[0];
				int lock_id = ctx->regs[1];
				ctx->regs[0] = kernelCvarWait(cvar_id, lock_id, ctx);
			}
		break;
		case YALNIX_PIPE_INIT:
			{
				PCB* currpcb = getHeadProcess(&gRunningProcessQ);
				memcpy(currpcb->m_uctx, ctx, sizeof(UserContext));
				int* pipe_idp = (int*)ctx->regs[0];
				memcpy(ctx, currpcb->m_uctx, sizeof(UserContext));
				int rc = kernelPipeInit(pipe_idp);
				ctx->regs[0] = rc;
			}
		break;
		case YALNIX_PIPE_READ:
			{
				PCB* currpcb = getHeadProcess(&gRunningProcessQ);
				memcpy(currpcb->m_uctx, ctx, sizeof(UserContext));
				int pipe_id = (int)ctx->regs[0];
				void* buff = (void*)ctx->regs[1];
				int len = (int)ctx->regs[2];
				int rc = kernelPipeRead(pipe_id, buff, len);
				memcpy(ctx, currpcb->m_uctx, sizeof(UserContext));
				ctx->regs[0] = rc;
				return;
			}
		break;
		case YALNIX_PIPE_WRITE:
			{
				PCB* currpcb = getHeadProcess(&gRunningProcessQ);
				memcpy(currpcb->m_uctx, ctx, sizeof(UserContext));
				int pipe_id = (int)ctx->regs[0];
				void* buff = (void*)ctx->regs[1];
				int len = (int)ctx->regs[2];
				if(len > PIPE_BUFFER_LEN) { TracePrintf(0, "ERROR: Writing to a pipe with length greater than its size\n"); ctx->regs[0] = ERROR; return; }
				int rc = kernelPipeWrite(pipe_id, buff, len);
				memcpy(ctx, currpcb->m_uctx, sizeof(UserContext));
				ctx->regs[0] = rc;
				return;
			}
		break;
		case YALNIX_RECLAIM:
			{
				int id = ctx->regs[0];
				ctx->regs[0] = kernelReclaim(id);
				return;
			}
		break;
		case YALNIX_CUSTOM_0:
			{
				PCB* currpcb = getHeadProcess(&gRunningProcessQ);
				memcpy(currpcb->m_uctx, ctx, sizeof(UserContext));
				int tty_id = ctx->regs[0];
				ctx->regs[0] = kernelPS(tty_id);
				return;
			}
		break;
		default:
			// all others are not implemented syscalls are not implemented.
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

	scheduleSleepingProcesses();
	processPendingPipeReadRequests();
	freeExitedProcesses();			// free the resources associated with exited processes

	// update the quantum of runtime for the current running process
	PCB* currpcb = getHeadProcess(&gRunningProcessQ);
  	currpcb->m_ticks++;
	if(currpcb->m_ticks > 1)
	{
		// schedule logic
		if(getHeadProcess(&gReadyToRunProcessQ) != NULL)
		{
			TracePrintf(0, "We have a process to schedule out\n");
			memcpy(currpcb->m_uctx, ctx, sizeof(UserContext));

			processDequeue(&gRunningProcessQ);
			processEnqueue(&gReadyToRunProcessQ, currpcb);
			PCB* nextpcb = getHeadProcess(&gReadyToRunProcessQ);
			if(nextpcb != NULL)
			{
				int rc = KernelContextSwitch(SwitchKCS, currpcb, nextpcb);
				if(rc == -1)
				{
					TracePrintf(0, "Kernel Context switch failed\n");
					exit(-1);
				}
			}
			else
			{
				TracePrintf(0, "ERROR: Didnot find another process to schedule - Inside : INTERRUPT_CLOCK\n");
			}

			// We just awoke.! Reset my time
			processRemove(&gReadyToRunProcessQ, currpcb);					// remove the process that was picked to run
			processEnqueue(&gRunningProcessQ, currpcb);
			swapPageTable(currpcb);
			memcpy(ctx, currpcb->m_uctx, sizeof(UserContext));
			return;
		}
	}
}

// Interrupt handler for illegal instruction
void interruptIllegal(UserContext* ctx)
{
	PCB* currpcb = getHeadProcess(&gRunningProcessQ);
	TracePrintf(0, "Illegal Interrupt Happened.\nKilling Process : %d\n", currpcb->m_pid);

	int status = ctx->regs[0];
	kernelExit(status, ctx);
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
		TracePrintf(0, "Memtrap for a page with invalid access permissions. Killing the process\n");
		kernelExit(ERROR, ctx);
	}

	// compute the locations of the page from the top of the VM
	// and also get the location of the brk of the process
	unsigned int loc = (unsigned int)ctx->addr;
	unsigned int brkloc = (unsigned int)currPCB->m_brk;
	unsigned int pg = (loc) / PAGESIZE;
	unsigned int brkpg = (brkloc) / PAGESIZE;
	pg -= gNumPagesR0;
	brkpg -= gNumPagesR0;
	if(currPCB->m_pagetable->m_pte[pg].valid == 0)
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

			unsigned int r1Top = gNumPagesR1;
			int currpg;
			for(currpg = r1Top; currpg >= pg; currpg--)
			{
				if(currPCB->m_pagetable->m_pte[currpg].valid == 1) continue;
				else
				{
					// started finding the invalid pages.
					FrameTableEntry* frame = getOneFreeFrame(&gFreeFramePool, &gUsedFramePool);
					if(frame != NULL)
					{
						currPCB->m_pagetable->m_pte[pg].valid = 1;
						currPCB->m_pagetable->m_pte[pg].prot = PROT_READ | PROT_WRITE;
						currPCB->m_pagetable->m_pte[pg].pfn = frame->m_frameNumber;
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
			kernelExit(ctx->regs[0], ctx);
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
	// report to user and abort the process
	int status = ctx->regs[0];
	kernelExit(status, ctx);
}

// Interrupt Handler for terminal recieve
void interruptTtyReceive(UserContext* ctx)
{
	int tty_id = ctx->code;
	// put the current running process into ready to run queues
	PCB* currpcb = processDequeue(&gRunningProcessQ);
	memcpy(currpcb->m_uctx, ctx, sizeof(UserContext));
	processEnqueue(&gReadyToRunProcessQ, currpcb);

	// pick the process that was doing a service requests
	TerminalRequest* head = &gTermRReqHeads[tty_id];
	TerminalRequest* req = head->m_next;
	if(req != NULL)
	{
		// read the bytes First
		int read = TtyReceive(tty_id, req->m_bufferR0, TERMINAL_MAX_LINE);
		req->m_serviced = read;
		PCB* nextpcb = req->m_pcb;
		if(nextpcb != NULL)
		{
			int rc = KernelContextSwitch(SwitchKCS, currpcb, nextpcb);
			if(rc == -1)
			{
				TracePrintf(0, "ERROR: context switch failed inside terminal receive interrupt handler\n");
			}
		}
		else
		{
			TracePrintf(0, "ERROR: No pcb to schedule : Inside : TTY_RECEIVE\n");
		}
	}

	// THis process which called the terminal process to push text to terminal
	// wakes up here again. We put ourselves again in running queue
	processRemove(&gReadyToRunProcessQ, currpcb);
	processEnqueue(&gRunningProcessQ, currpcb);
	swapPageTable(currpcb);
	memcpy(ctx, currpcb->m_uctx, sizeof(UserContext));
	return;
}

// Interrupt Handler for terminal transmit
void interruptTtyTransmit(UserContext* ctx)
{
	// some request was completed.
	// find that process and put it in ready to run queue
	int tty_id = ctx->code;

	// put the current running process into ready to run queues
	/*
	PCB* currpcb = processDequeue(&gRunningProcessQ);
	memcpy(currpcb->m_uctx, ctx, sizeof(UserContext));
	processEnqueue(&gReadyToRunProcessQ, currpcb);

	// pick the process that was doing a service requests

	if(req != NULL)
	{
		PCB* nextpcb = req->m_pcb;
		if(nextpcb != NULL)
		{
			int rc = KernelContextSwitch(SwitchKCS, currpcb, nextpcb);
			if(rc == -1)
			{
				TracePrintf(0, "ERROR: context switch failed inside terminal receive interrupt handler\n");
			}
		}
		else
		{
			TracePrintf(0, "ERROR: No PCB - Inside : TTY_TRANSMIT\n");
		}
	}

	// THis process which called the terminal process to push text to terminal
	// wakes up here again. We put ourselves again in running queue
	processRemove(&gReadyToRunProcessQ, currpcb);
	processEnqueue(&gRunningProcessQ, currpcb);
	swapPageTable(currpcb);
	memcpy(ctx, currpcb->m_uctx, sizeof(UserContext));
	return;
	*/
	TerminalRequest* head = &gTermWReqHeads[tty_id];
	TerminalRequest* req = head->m_next;
	if(req != NULL)
	{
		PCB* pcb = req->m_pcb;
		processRemove(&gWriteBlockedQ, pcb);
		processEnqueue(&gReadyToRunProcessQ, pcb);
	}
	return;
}

// This is a dummy interrupt handler that does nothing.
// for the rest of the IVT entries
void interruptDummy(UserContext* ctx)
{

}
