#include <interrupt_handler.h>
#include <process.h>
#include <syscalls.h>
#include <yalnix.h>

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
        case YALNIX_WAIT:
            {
                // move the process to the wait queue
                // try to find some other process to run in its place
            }
        break;
        case YALNIX_BRK:
            {
                // update the logic for brk within the process's addresspace
            }
            break;
        case YALNIX_GETPID:
            {
				int pid = kernelGetPid();
				ctx->regs[0] = (u_long)pid;
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
	if(debug == 2)
		TracePrintf(0, "WHOA.!!\n");

	// update the quantum of runtime for the current running process
	PCB* currRunningPcb = gRunningProcessQ.m_next;
	currRunningPcb->m_ticks++;

	if(currRunningPcb->m_kctx == NULL)
	{
		// get the first kernel context
		int rc = KernelContextSwitch(MyKCS, currRunningPcb, NULL);
		if(rc == -1)
		{
			TracePrintf(0, "Getting first context failed");
			exit(-1);
		}
	}
	
	// if this process has run for too long 
	// say 3 ticks, then swap it with a different process in the ready to run queue
	if(currRunningPcb->m_ticks > 2)
	{
		// schedule logic
		if(gReadyToRunProcesssQ.m_next != NULL)
		{
			TracePrintf(0, "We have a process to schedule out");
			PCB* nextPCB = gReadyToRunProcesssQ.m_next;
			int rc = KernelContextSwitch(MyKCS, currRunningPcb, nextPCB);
			if(rc == -1)
			{
				TracePrintf(0, "Kernel Context switch failed\n");
				exit(-1);
			}

			// swap the two pcbs
			gRunningProcessQ.m_next = nextPCB;
			gReadyToRunProcesssQ.m_next = currRunningPcb;

			// set the page tables and registers
			// flush the region0 - stack frames and region1 - frames
			WriteRegister(REG_PTBR0, (unsigned int)nextPCB->m_pt->m_pte);
			WriteRegister(REG_PTLR0, (NUM_VPN >> 1));
			WriteRegister(REG_PTBR1, (unsigned int)nextPCB->m_pt->m_pte + (NUM_VPN >> 1));
			WriteRegister(REG_PTLR1, (NUM_VPN >> 1));
			WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
			WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

			// update the user context
			currRunningPcb->m_ticks = 0;	// reset ticks
			ctx = nextPCB->m_uctx;
			debug = 2;
		}
		else if(currRunningPcb->m_kctx == NULL)
		{
			// get the current kernel context to have the context available for successful context switch
			KernelContextSwitch(MyKCS, currRunningPcb, NULL);
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
	// check for stack brk or dynamic brk
	// if stack brk, then allocate a new pages
		// and map it to page tables.
		// return back to continue execution of the current process
	// if heap brk required
		// check if frames are available for allocation
		// if frames are available, allocate the frame,
		// map the frames to pages in pagetable entry
		// update the sbrk/brk values for the process
	// If at any point the stack grows into the heap,
		// log and quit the process.
		
	TracePrintf(0, "TRAP_MEMORY.\n");
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