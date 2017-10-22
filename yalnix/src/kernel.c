#include <filesystem.h>
#include <hardware.h>
#include <interrupt_handler.h>
#include <load_info.h>
#include <pagetable.h>
#include <process.h>
#include <yalnix.h>
#include <yalnixutils.h>

// some extern functions
extern int LoadProgram(char *name, char *args[], PCB* pcb);

// set the global pid to zero
int gPID = 1;
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

// The global PCBs
ProcessNode gRunningProcessQ;
ProcessNode gReadyToRunProcesssQ;
ProcessNode gWaitProcessQ;
ProcessNode gTerminatedProcessQ;

// interrupt vector table
// we have 7 types of interrupts
void (*gIVT[TRAP_VECTOR_SIZE])(UserContext*);

int gVMemEnabled = -1;			// global flag to keep track of the enabling of virtual memory

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
	gRunningProcessQ.m_next = NULL; gRunningProcessQ.m_prev = NULL;
	gTerminatedProcessQ.m_next = NULL; gTerminatedProcessQ.m_prev = NULL;
	gReadyToRunProcesssQ.m_next = NULL; gReadyToRunProcesssQ.m_prev = NULL;
	gWaitProcessQ.m_next = NULL; gWaitProcessQ.m_prev = NULL;
	
	// Set the page table entries for the kernel in the correct register before enabling
	// VM
	WriteRegister(REG_PTBR0, (unsigned int)gKernelPageTable.m_pte);
	WriteRegister(REG_PTLR0, gNumPagesR0);
	WriteRegister(REG_PTBR1, (unsigned int)(gKernelPageTable.m_pte + gNumPagesR0));
	WriteRegister(REG_PTLR1, gNumPagesR0);

	// enable virtual memory
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

	// copy the kernel's region0 page entries into this process's ptes
	for(i = 0; i < gNumPagesR0; i++)
	{
		if(gKernelPageTable.m_pte[i].valid == 1)
			pInitPT->m_pte[i] = gKernelPageTable.m_pte[i];
	}

	// Create a PCB entry 
	PCB* pInitPCB = (PCB*)malloc(sizeof(PCB));
	if(pInitPCB == NULL)
	{
		TracePrintf(0, "Unable to create pcb entry for idle process");
		exit(-1);
	}

	// Create a user context for the idle program
	UserContext* pInitUC = (UserContext*)malloc(sizeof(UserContext));
	if(pInitUC == NULL)
	{
		TracePrintf(0, "Unable to create user context for idle process");
		exit(-1);
	}
	else
	{
		// write out the current context's data into this
		pInitUC->vector = uctx->vector;
		pInitUC->code = uctx->code;
		pInitUC->addr = uctx->addr;
		pInitUC->pc = uctx->pc;
		pInitUC->sp = uctx->sp;
		pInitUC->ebp = uctx->ebp;
		for(i = 0; i < GREGS; i++)
			pInitUC->regs[i] = uctx->regs[i];
	}

	// set the entries in the pcb
	pInitPCB->m_pid = gPID++;
	pInitPCB->m_ppid = pInitPCB->m_pid;		// for now this is its own parent
	pInitPCB->m_pt = pInitPT;
	pInitPCB->m_uctx = pInitUC;
	pInitPCB->m_state = PROCESS_RUNNING;
	pInitPCB->m_ticks = 0;					// 0 for now.

	// add the pcb to running for now
	// NOTE: we should be moving this to ready-to-run queue and let the scheduler actually pick this process
	//       and move it to running queue. But we do this for now.
	gRunningProcessQ.m_prev = NULL;
	gRunningProcessQ.m_next = pInitPCB;

	// swap out the page tables
	// We need to do this because, further virtual address references have to go to the correct frames
	// load program basically copies text, data into virtual addresses. so the pagetables should reflect this.
	WriteRegister(REG_PTBR0, (unsigned int)pInitPCB->m_pt->m_pte);
	WriteRegister(REG_PTBR1, (unsigned int)(pInitPCB->m_pt->m_pte + gNumPagesR0));
	WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

	// Call load program
	int statusCode = LoadProgram(argv[0], &argv[1], pInitPCB);

	if(statusCode != SUCCESS)
	{
		TracePrintf(0, "Error loading the idle process\n");
		exit(-1);
	}

	// start running from this process.
	uctx->pc = pInitPCB->m_uctx->pc;
	uctx->sp = pInitPCB->m_uctx->sp;
}
