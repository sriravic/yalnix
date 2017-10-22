#include <interrupt_handler.h>
#include <syscalls.h>
#include <yalnix.h>

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
				ctx->regs[0] = pid;
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