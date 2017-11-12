#include <filesystem.h>
#include <hardware.h>
#include <interrupt_handler.h>
#include <load_info.h>
#include <pagetable.h>
#include <process.h>
#include <terminal.h>
#include <yalnix.h>
#include <yalnixutils.h>
#include <synchronization.h>

// convenient macros
#define INIT_QUEUE_HEADS(A) { A.m_head = NULL; A.m_tail = NULL; }

// some extern functions
extern int LoadProgram(char *name, char *args[], PCB* pcb);

// set the global pid to zero
int gPID = 0;
int gSID = 0;
void* gKernelBrk;

// the global kernel page table
PageTable gKernelPageTable;
unsigned int gKernelSP;					// the address of the kernel stack pointer.

// the global free frame lists
FrameTableEntry gFreeFramePool;
FrameTableEntry gUsedFramePool;

unsigned int gNumPagesR0 = NUM_VPN >> 1;
unsigned int gNumPagesR1 = NUM_VPN >> 1;

// kernel text and data
unsigned int gKernelDataStart;
unsigned int gKernelDataEnd;
unsigned int gNumFramesBeforeVM;			// the number of frames that were used before VM was actually enabled.

// The global PCBs
PCBQueue gRunningProcessQ;
PCBQueue gReadyToRunProcessQ;
PCBQueue gWaitProcessQ;
PCBQueue gTerminatedProcessQ;
PCBQueue gSleepBlockedQ;
PCBQueue gReadBlockedQ;
PCBQueue gReadFinishedQ;
PCBQueue gWriteBlockedQ;
PCBQueue gWriteFinishedQ;

// The global synchronization queues
LockQueue gLockQueue;
PipeQueue gPipeQueue;
PipeReadWaitQueue gPipeReadWaitQueue;

// interrupt vector table
// we have 7 types of interrupts
void (*gIVT[TRAP_VECTOR_SIZE])(UserContext*);

int gVMemEnabled = -1;			// global flag to keep track of the enabling of virtual memory

// Terminal Requests header nodes
TerminalRequest gTermWReqHeads[NUM_TERMINALS];
TerminalRequest gTermRReqHeads[NUM_TERMINALS];

int SetKernelBrk(void* addr)
{
	if(gVMemEnabled == -1)
	{
		// virtual memory has not yet been set.
		TracePrintf(0, "SetKernelBrk : 0x%08X\n", addr);
		gKernelBrk = addr;
		return 0;
	}
	else
	{
		// virtual memory has been enabled.
		// check for correct frames and update the kernel heap page tables
		TracePrintf(0, "SetkernelBrk called after VM enabled\n");
		unsigned int newBrkAddr = (unsigned int)addr;
		unsigned int oldBrkAddr = (unsigned int)gKernelBrk;
		unsigned int oldBrkPg = oldBrkAddr / PAGESIZE;
		unsigned int newBrkPg = newBrkAddr / PAGESIZE;
		if(newBrkAddr > oldBrkAddr)
		{
			// the heap was grown
			// set the address to the new address
			int pg;
			for(pg = newBrkPg; pg <= newBrkPg; pg++)
			{
				FrameTableEntry* frame = getOneFreeFrame(&gFreeFramePool, &gUsedFramePool);
				if(frame != NULL)
				{
					gKernelPageTable.m_pte[pg].valid = 1;
					gKernelPageTable.m_pte[pg].prot = PROT_READ | PROT_WRITE;
					gKernelPageTable.m_pte[pg].pfn = frame->m_frameNumber;
				}
				else
				{
					TracePrintf(0, "Unable to find new frames for kernel brk\n");
				}
			}
		}
		else
		{
			TracePrintf(0, "Freeing up frames from R0 since brk was lower than original brk");
			int pg;
			// Free up the phyiscal frames except the ones we have not allocated to before initializing the VM
			for(pg = oldBrkPg; pg > oldBrkPg && pg > gNumFramesBeforeVM; pg--)
			{
				int frame = gKernelPageTable.m_pte[pg].pfn;
				freeOneFrame(&gFreeFramePool, &gUsedFramePool, frame);
				gKernelPageTable.m_pte[pg].valid = 0;
				gKernelPageTable.m_pte[pg].prot = 0;
				gKernelPageTable.m_pte[pg].pfn = 0;
			}
		}

		// update the brk
		gKernelBrk = addr;
	}
}

void SetKernelData(void* _KernelDataStart, void* _KernelDataEnd)
{
    gKernelBrk = _KernelDataEnd;
    TracePrintf(0, "DataStart  : 0x%08X\n", _KernelDataStart);
    TracePrintf(0, "DataEnd    : 0x%08X\n", _KernelDataEnd);

	gKernelDataStart = (unsigned int)_KernelDataStart;
	gKernelDataEnd = (unsigned int)_KernelDataEnd;
}

KernelContext* MyKCS(KernelContext* kc_in, void* curr_pcb_p, void* next_pcb_p)
{
	PCB* currPCB = (PCB*)curr_pcb_p;
	PCB* nextPCB = (PCB*)next_pcb_p;

	if(nextPCB == NULL && currPCB->m_kctx == NULL)
	{
		// allocate a new chunck for the storing the state of the kernel context
		KernelContext* ctx = (KernelContext*)malloc(sizeof(KernelContext));

		// this was just to get the current context
		// copy the context and return the same
		// and fill in the entries of the currPCB
		memcpy(ctx, kc_in, sizeof(KernelContext));
		currPCB->m_kctx = ctx;
		return ctx;
	}
	else if(currPCB != NULL && nextPCB != NULL)
	{
		// We store the current state in teh current process
		memcpy(currPCB->m_kctx, kc_in, sizeof(KernelContext));

		// return 'to-be-run' context
		//if(nextPCB->m_kctx == NULL)
		return currPCB->m_kctx;
		//else return nextPCB->m_kctx;
	}
	else
	{
		TracePrintf(0, "Weird scenario - Returning NULL kernel context\n");
		return NULL;
	}
}

void KernelStart(char** argv, unsigned int pmem_size, UserContext* uctx)
{
    TracePrintf(0, "KernelStart Function\n");

	// zero out the kernel page tables
	int i;
	memset(gKernelPageTable.m_pte, 0, sizeof(PageTable));

    // parse the argvs
    int argc = 0;
    while(argv[argc] != NULL)
    {
        TracePrintf(0, "\t Argv : %s\n", argv[argc++]);
    }

	TracePrintf(0, "Available memory : %u MB\n", getMB(pmem_size));

	// initialize the IVT
	// only 7 are valid
	// setting the rest to the dummy interrupt handler
	gIVT[0] = (void*)interruptKernel;
	gIVT[1] = (void*)interruptClock;
	gIVT[2] = (void*)interruptIllegal;
	gIVT[3] = (void*)interruptMemory;
	gIVT[4] = (void*)interruptMath;
	gIVT[5] = (void*)interruptTtyReceive;
	gIVT[6] = (void*)interruptTtyTransmit;
	for(i = 7; i < TRAP_VECTOR_SIZE; i++)
		gIVT[i] = (void*)interruptDummy;

	unsigned int ivtBaseRegAddr = (unsigned int)(&(gIVT[0]));
	TracePrintf(0, "Base IVT Register address : 0x%08X\n", ivtBaseRegAddr);
	WriteRegister(REG_VECTOR_BASE, ivtBaseRegAddr);

	// map the initial page tables and frames
	unsigned int TOTAL_FRAMES = pmem_size / PAGESIZE;		// compute the total number of frames
	unsigned int dataStart = (unsigned int)gKernelDataStart;
	unsigned int dataEnd = (unsigned int)gKernelDataEnd;
	unsigned int dataStartRounded = UP_TO_PAGE(dataStart);
	unsigned int dataEndRounded = UP_TO_PAGE(dataEnd);
	unsigned int textEnd = DOWN_TO_PAGE(dataStart);
	unsigned int TEXT_FRAME_END_PAGENUM = textEnd / PAGESIZE;
	unsigned int DATA_FRAME_END_PAGENUM = dataEnd / PAGESIZE;
	unsigned int NUM_DATA_FRAMES_IN_USE = DATA_FRAME_END_PAGENUM - TEXT_FRAME_END_PAGENUM;
	unsigned int NUM_FRAMES_IN_USE = DATA_FRAME_END_PAGENUM;

	TracePrintf(0, "Data End Address : 0x%08x\n", dataEndRounded);
	TracePrintf(0, "Global brk : 0x%08x\n",UP_TO_PAGE((unsigned int)gKernelBrk));
	TracePrintf(0, "Text Frame End : %u\n", TEXT_FRAME_END_PAGENUM);
	TracePrintf(0, "Data Frame End : %u\n", DATA_FRAME_END_PAGENUM);

	TracePrintf(0, "Total Physical Frames : %u\n", TOTAL_FRAMES);
	TracePrintf(0, "Total Frames In USE : %u\n", NUM_FRAMES_IN_USE);
	TracePrintf(0, "Total Remaining pages : %u\n", TOTAL_FRAMES - NUM_FRAMES_IN_USE);

	// first initialize the pools
	gUsedFramePool.m_head = 1; gFreeFramePool.m_head = 1;

	// allocate the first required 'N' frames in allocated
	int frameNum;
	FrameTableEntry* curr = &gUsedFramePool;
	for(frameNum = 0; frameNum < NUM_FRAMES_IN_USE; frameNum++)
	{
		FrameTableEntry* next = (FrameTableEntry*)malloc(sizeof(FrameTableEntry));
		if(next != NULL)
		{
			next->m_frameNumber = frameNum;
			next->m_head = 0;
			next->m_next = NULL;
			curr->m_next = next;
			curr = next;
		}
		else
		{
			TracePrintf(0, "Unable to allocate memory for used frame pool list - frame : %d\n", frameNum);
			exit(-1);
		}
	}

	curr = &gFreeFramePool;
	for(frameNum = NUM_FRAMES_IN_USE; frameNum < TOTAL_FRAMES; frameNum++)
	{
		FrameTableEntry* next = (FrameTableEntry*)malloc(sizeof(FrameTableEntry));
		if(next != NULL)
		{
			next->m_frameNumber = frameNum;
			next->m_head = 0;
			next->m_next = NULL;
			curr->m_next = next;
			curr = next;
		}
		else
		{
			TracePrintf(0, "Unable to allocate memory for free frame pool list - frame : %d\n", frameNum);
			exit(-1);
		}
	}

	// update the heap allocations if any
	unsigned int NUM_HEAP_FRAMES_IN_USE = (UP_TO_PAGE((unsigned int)gKernelBrk) - dataEndRounded) / PAGESIZE;
	TracePrintf(0, "Total Heap Frames : %u\n", NUM_HEAP_FRAMES_IN_USE);

	// Initialize the page tables for the kernel
	// map the used pages to used frames as one-one mapping
	for(i = 0; i < TEXT_FRAME_END_PAGENUM; i++)
	{
		gKernelPageTable.m_pte[i].valid = 1;
		gKernelPageTable.m_pte[i].prot = PROT_READ|PROT_EXEC;
		gKernelPageTable.m_pte[i].pfn = i;
	}
	for(i = TEXT_FRAME_END_PAGENUM; i < NUM_FRAMES_IN_USE; i++)
	{
		gKernelPageTable.m_pte[i].valid = 1;
		gKernelPageTable.m_pte[i].prot = PROT_READ|PROT_WRITE;
		gKernelPageTable.m_pte[i].pfn = i;
	}

	// Allocate one page for kernel stack region
	// This has to be in the same spot
	unsigned int stackIndex = (KERNEL_STACK_BASE / PAGESIZE);
	TracePrintf(0, "Kernel stack index : %u\n", stackIndex);

	// Find two frames that are in the free list at STACK_BASE and STACK_BASE - 1
	// and move them to used list. and allocate the pte entries to these frames
	// we need this because VM is not enabled yet and hence we have to map one-one
	FrameTableEntry* prev = gFreeFramePool.m_next;
	curr = prev;
	while(curr != NULL)
	{
		if(curr->m_frameNumber == stackIndex)
		{
			// remove this frame and the next frame and move it to the other used pool
			FrameTableEntry* f1 = curr;
			FrameTableEntry* f2 = curr->m_next;

			TracePrintf(0, "Stack Frame pfn : %u\n", f1->m_frameNumber);
			TracePrintf(0, "Stack frame pfn : %u\n", f2->m_frameNumber);

			// reassign the pointers
			prev->m_next = f2->m_next;

			// move to the end of the allocated list
			FrameTableEntry* prevAlloc = gUsedFramePool.m_next;
			FrameTableEntry* currAlloc = prevAlloc;
			while(currAlloc != NULL)
			{
				prevAlloc = currAlloc;
				currAlloc = currAlloc->m_next;
			}

			prevAlloc->m_next = f1;
			prevAlloc->m_next->m_next = f2;
			f2->m_next = NULL;

			// set ptes
			gKernelPageTable.m_pte[stackIndex].valid = 1;
			gKernelPageTable.m_pte[stackIndex].prot = PROT_READ|PROT_WRITE;
			gKernelPageTable.m_pte[stackIndex].pfn = f1->m_frameNumber;
			gKernelPageTable.m_pte[stackIndex + 1].valid = 1;
			gKernelPageTable.m_pte[stackIndex + 1].prot = PROT_READ|PROT_WRITE;
			gKernelPageTable.m_pte[stackIndex + 1].pfn = f2->m_frameNumber;
			break;
		}
		else
		{
			prev = curr;
			curr = curr->m_next;
		}
	}

	// create the initial process queues
	INIT_QUEUE_HEADS(gRunningProcessQ);
	INIT_QUEUE_HEADS(gTerminatedProcessQ);
	INIT_QUEUE_HEADS(gReadyToRunProcessQ);
	INIT_QUEUE_HEADS(gWaitProcessQ);
	INIT_QUEUE_HEADS(gSleepBlockedQ);
	INIT_QUEUE_HEADS(gReadBlockedQ);
	INIT_QUEUE_HEADS(gReadFinishedQ);
	INIT_QUEUE_HEADS(gWriteBlockedQ);
	INIT_QUEUE_HEADS(gWriteFinishedQ);

	// create initial synchronization queues
	INIT_QUEUE_HEADS(gLockQueue);
	INIT_QUEUE_HEADS(gPipeQueue);
	INIT_QUEUE_HEADS(gPipeReadWaitQueue);

	// Set the page table entries for the kernel in the correct register before enabling VM
	WriteRegister(REG_PTBR0, (unsigned int)gKernelPageTable.m_pte);
	WriteRegister(REG_PTLR0, gNumPagesR0);
	WriteRegister(REG_PTBR1, (unsigned int)(gKernelPageTable.m_pte + gNumPagesR0));
	WriteRegister(REG_PTLR1, gNumPagesR0);

	// initialize the terminal write + read heads heads
	int term;
	for(term = 0; term < NUM_TERMINALS; term++)
	{
		gTermWReqHeads[term].m_code = TERM_REQ_NONE;
		gTermWReqHeads[term].m_pcb = NULL;
		gTermWReqHeads[term].m_bufferR0 = NULL;
		gTermWReqHeads[term].m_bufferR1 = NULL;
		gTermWReqHeads[term].m_len = 0;
		gTermWReqHeads[term].m_serviced = 0;
		gTermWReqHeads[term].m_remaining = 0;
		gTermWReqHeads[term].m_requestInitiated = 0;
		gTermWReqHeads[term].m_next = NULL;
	}

	for(term = 0; term < NUM_TERMINALS; term++)
	{
		gTermRReqHeads[term].m_code = TERM_REQ_NONE;
		gTermRReqHeads[term].m_pcb = NULL;
		gTermRReqHeads[term].m_bufferR0 = NULL;
		gTermRReqHeads[term].m_bufferR1 = NULL;
		gTermRReqHeads[term].m_len = 0;
		gTermRReqHeads[term].m_serviced = 0;
		gTermRReqHeads[term].m_remaining = 0;
		gTermRReqHeads[term].m_requestInitiated = 0;
		gTermRReqHeads[term].m_next = NULL;
	}

	// enable virtual memory
	gNumFramesBeforeVM = (unsigned int)gKernelBrk / PAGESIZE;
	WriteRegister(REG_VM_ENABLE, 1);
	gVMemEnabled = 1;

	// Load the init program
	// Create a page table for the new idle process
	PageTable* pInitPT = (PageTable*)malloc(sizeof(PageTable));
	if(pInitPT == NULL)
	{
		TracePrintf(0, "unable to create page table for idle process");
		exit(-1);
	}
	else
	{
		memset(pInitPT, 0, sizeof(PageTable));
	}

	// copy the kernel's region0 page - STACK FRAME entries into this process's ptes
	for(i = 0; i < gNumPagesR0 - 2; i++)
	{
		if(gKernelPageTable.m_pte[i].valid == 1)
			pInitPT->m_pte[i] = gKernelPageTable.m_pte[i];
	}

	// allocate additional two frames for kernel stack of the new process
	// each process has its own kernel stack that is unique to itself.
	// it does not share that with other processes.
	FrameTableEntry* kstack1 = getOneFreeFrame(&gFreeFramePool, &gUsedFramePool);
	FrameTableEntry* kstack2 = getOneFreeFrame(&gFreeFramePool, &gUsedFramePool);
	pInitPT->m_pte[stackIndex + 0].valid = 1; pInitPT->m_pte[stackIndex + 0].prot = PROT_READ | PROT_WRITE; pInitPT->m_pte[stackIndex + 0].pfn = kstack1->m_frameNumber;
	pInitPT->m_pte[stackIndex + 1].valid = 1; pInitPT->m_pte[stackIndex + 1].prot = PROT_READ | PROT_WRITE; pInitPT->m_pte[stackIndex + 1].pfn = kstack2->m_frameNumber;

	// Create a PCB entry
	PCB* pInitPCB = (PCB*)malloc(sizeof(PCB));
	if(pInitPCB == NULL)
	{
		TracePrintf(0, "Unable to create pcb entry for idle process");
		exit(-1);
	}

	// create a child exit data queue
	EDQueue* initEDQ = (EDQueue*)malloc(sizeof(EDQueue));
	if(initEDQ == NULL)
	{
		TracePrintf(0, "Unable to create exit data queue for idle process");
		exit(-1);
	}

	// Create a user context for the init program
	UserContext* pInitUC = (UserContext*)malloc(sizeof(UserContext));
	if(pInitUC == NULL)
	{
		TracePrintf(0, "Unable to create user context for init process");
		exit(-1);
	}
	else
	{
		// write out the current context's data into this user context
		pInitUC->vector = uctx->vector;
		pInitUC->code = uctx->code;
		pInitUC->addr = uctx->addr;
		pInitUC->pc = uctx->pc;
		pInitUC->sp = uctx->sp;
		pInitUC->ebp = uctx->ebp;
		for(i = 0; i < GREGS; i++)
			pInitUC->regs[i] = uctx->regs[i];
	}

	// set the entries in the corresponding PCB
	pInitPCB->m_pid = gPID++;
	pInitPCB->m_ppid = pInitPCB->m_pid;		// for now this is its own parent
	pInitPCB->m_pt = pInitPT;
	pInitPCB->m_uctx = pInitUC;
	pInitPCB->m_kctx = NULL;
	pInitPCB->m_state = PROCESS_RUNNING;
	pInitPCB->m_ticks = 0;					// 0 for now.
	pInitPCB->m_timeToSleep = 0;
	pInitPCB->m_next = NULL;
	pInitPCB->m_prev = NULL;
	pInitPCB->m_edQ = initEDQ;
	pInitPCB->m_iodata = NULL;

	// add the pcb to running for now
	// NOTE: we should be moving this to ready-to-run queue and let the scheduler actually pick this process
	//       and move it to running queue. But we do this for now.
	//gRunningProcessQ.m_prev = NULL;
	//gRunningProcessQ.m_next = pInitPCB;
	processEnqueue(&gRunningProcessQ, pInitPCB);

	// swap out the page tables
	// We need to do this because, further virtual address references have to go to the correct frames
	// load program basically copies text, data into virtual addresses. so the pagetables should reflect this.
	swapPageTable(pInitPCB);

	// Call load program
	int statusCode = LoadProgram(argv[0], &argv[1], pInitPCB);

	if(statusCode != SUCCESS)
	{
		TracePrintf(0, "Error loading the init process\n");
		exit(-1);
	}
	else
	{
		// call switch kernel context to get the current kernel context
		KernelContextSwitch(MyKCS, pInitPCB, NULL);
	}

	// create the idle program also as above
	// but add it to the ready to run programs list
	// also make sure to set the correct page tables before letting this happen so that load can succeed
	// after that, reset the page table registers to start executing in init's context.
	// Create a page table for the new idle process
	PageTable* pIdlePT = (PageTable*)malloc(sizeof(PageTable));
	if(pIdlePT == NULL)
	{
		TracePrintf(0, "unable to create page table for idle process");
		exit(-1);
	}
	else
	{
		memset(pIdlePT, 0, sizeof(PageTable));
	}

	// copy the kernel's region0 page - STACK FRAME entries into this process's ptes
	for(i = 0; i < gNumPagesR0 - 2; i++)
	{
		if(gKernelPageTable.m_pte[i].valid == 1)
			pIdlePT->m_pte[i] = gKernelPageTable.m_pte[i];
	}

	// allocate additional two frames for kernel stack of the new process
	// each process has its own kernel stack that is unique to itself.
	// it does not share that with other processes.
	kstack1 = getOneFreeFrame(&gFreeFramePool, &gUsedFramePool);
	kstack2 = getOneFreeFrame(&gFreeFramePool, &gUsedFramePool);
	pIdlePT->m_pte[stackIndex + 0].valid = 1; pIdlePT->m_pte[stackIndex + 0].prot = PROT_READ | PROT_WRITE; pIdlePT->m_pte[stackIndex + 0].pfn = kstack1->m_frameNumber;
	pIdlePT->m_pte[stackIndex + 1].valid = 1; pIdlePT->m_pte[stackIndex + 1].prot = PROT_READ | PROT_WRITE; pIdlePT->m_pte[stackIndex + 1].pfn = kstack2->m_frameNumber;

	// Create a PCB entry
	PCB* pIdlePCB = (PCB*)malloc(sizeof(PCB));
	if(pIdlePCB == NULL)
	{
		TracePrintf(0, "Unable to create pcb entry for idle process");
		exit(-1);
	}

	// create a child exit data queue
	EDQueue* idleEDQ = (EDQueue*)malloc(sizeof(EDQueue));
	if(idleEDQ == NULL)
	{
		TracePrintf(0, "Unable to create exit data queue for idle process");
		exit(-1);
	}

	// Create a user context for the idle program
	UserContext* pIdleUC = (UserContext*)malloc(sizeof(UserContext));
	if(pIdleUC == NULL)
	{
		TracePrintf(0, "Unable to create user context for idle process");
		exit(-1);
	}
	else
	{
		// write out the current context's data into this user context
		pIdleUC->vector = uctx->vector;
		pIdleUC->code = uctx->code;
		pIdleUC->addr = uctx->addr;
		pIdleUC->pc = uctx->pc;
		pIdleUC->sp = uctx->sp;
		pIdleUC->ebp = uctx->ebp;
		for(i = 0; i < GREGS; i++)
			pIdleUC->regs[i] = uctx->regs[i];
	}

	// set the entries in the corresponding PCB
	pIdlePCB->m_pid = gPID++;
	pIdlePCB->m_ppid = pInitPCB->m_pid;		// for now this is its own parent
	pIdlePCB->m_pt = pIdlePT;
	pIdlePCB->m_uctx = pIdleUC;
	pIdlePCB->m_kctx = NULL;
	pIdlePCB->m_state = PROCESS_READY;
	pIdlePCB->m_ticks = 0;					// 0 for now.
	pIdlePCB->m_timeToSleep = 0;
	pIdlePCB->m_next = NULL;
	pIdlePCB->m_prev = NULL;
	pIdlePCB->m_edQ = idleEDQ;
	pInitPCB->m_iodata = NULL;

	// reset to idle's pagetables for successfulyl loading
	swapPageTable(pIdlePCB);

	char idleprog[] = "testpipe";
	char* tempargs[] = {NULL};
	statusCode = LoadProgram(idleprog, tempargs, pIdlePCB);

	if(statusCode != SUCCESS)
	{
		TracePrintf(0, "Error loading the idle process\n");
		exit(-1);
	}
	else
	{
		//KernelContextSwitch(MyKCS, pIdlePCB, NULL);
	}

	// add this to ready to run queue
	processEnqueue(&gReadyToRunProcessQ, pIdlePCB);

	// Reset to init's page tables
	swapPageTable(pInitPCB);

	// start running from this process.
	uctx->pc = pInitPCB->m_uctx->pc;
	uctx->sp = pInitPCB->m_uctx->sp;
}
