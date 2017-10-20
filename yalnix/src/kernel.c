#include <yalnix.h>
#include <filesystem.h>
#include <hardware.h>
#include <load_info.h>
#include <interrupt_handler.h>
#include <pagetable.h>

void* globalBrk;

// the global kernel page table
PageTableEntry gKernelPageTable[NUM_VPN];

// the global free frame lists
FrameTableEntry gFreeFramePool;
FrameTableEntry gUsedFramePool;

unsigned int gNumPagesR0 = NUM_VPN >> 1;
unsigned int gNumPagesR1 = NUM_VPN >> 1;

// kernel text and data
unsigned int gKernelDataStart;
unsigned int gKernelDataEnd;

// interrupt vector table
// we have 7 types of interrupts
void (*gIVT[TRAP_VECTOR_SIZE])(UserContext*);

int gVMemEnabled = -1;			// global flag to keep track of the enabling of virtual memory

PageTableEntry* gPageTable;	// initial page tables

unsigned int getKB(unsigned int size) { return size >> 10; }
unsigned int getMB(unsigned int size) { return size >> 20; }
unsigned int getGB(unsigned int size) { return size >> 30; }

int SetKernelBrk(void* addr)
{
	if(gVMemEnabled == -1)
	{
		// virtual memory has not yet been set.
		TracePrintf(0, "SetKernelBrk : 0x%08X\n", addr);
		globalBrk = addr;
		return 0;
	}
	else
	{
		// virtual memory has been enabled.
		// the logic will be different
	}
}

void SetKernelData(void* _KernelDataStart, void* _KernelDataEnd)
{
    globalBrk = _KernelDataEnd;
    TracePrintf(0, "DataStart  : 0x%08X\n", _KernelDataStart);
    TracePrintf(0, "DataEnd    : 0x%08X\n", _KernelDataEnd);

	gKernelDataStart = (unsigned int)_KernelDataStart;
	gKernelDataEnd = (unsigned int)_KernelDataEnd;
}

void DoIdle()
{
	while(1)
	{
		TracePrintf(1, "DoIdle\n");
		Pause();
	}
}

void KernelStart(char** argv, unsigned int pmem_size, UserContext* uctx)
{
    TracePrintf(0, "KernelStart Function\n");
    
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
	int i;
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
	unsigned int NUM_TEXT_FRAMES_IN_USE = textEnd / PAGESIZE;
	unsigned int NUM_DATA_FRAMES_IN_USE = (dataEndRounded - dataStartRounded) / PAGESIZE;

	// first initialize the pools
	gUsedFramePool.m_head = 1; gFreeFramePool.m_head = 1;
	
	// allocate the first required 'N' frames in allocated
	int frameNum;
	FrameTableEntry curr = gUsedFramePool;
	for(frameNum = 0; frameNum < NUM_TEXT_FRAMES_IN_USE + NUM_DATA_FRAMES_IN_USE; frameNum++)
	{
		FrameTableEntry* next = (FrameTableEntry*)malloc(sizeof(FrameTableEntry));
		if(next != NULL)
		{
			next->m_frameNumber = frameNum;
			next->m_head = 0;
			next->m_next = NULL;
			curr.m_next = next;
			curr = *next;
		}
		else
		{
			TracePrintf(0, "Unable to allocate memory for used frame pool list - frame : %d\n", frameNum);
			exit(-1);
		}
	}

	curr = gFreeFramePool;
	for(frameNum = NUM_TEXT_FRAMES_IN_USE + NUM_DATA_FRAMES_IN_USE; frameNum < TOTAL_FRAMES; frameNum++)
	{
		FrameTableEntry* next = (FrameTableEntry*)malloc(sizeof(FrameTableEntry));
		if(next != NULL)
		{
			next->m_frameNumber = frameNum;
			next->m_head = 0;
			next->m_next = NULL;
			curr.m_next = next;
			curr = *next;
		}
		else
		{
			TracePrintf(0, "Unable to allocate memory for free frame pool list - frame : %d\n", frameNum);
			exit(-1);
		}
	}

	// Initialize the page tables for the kernel
	// map the used pages to used frames as one-one mapping
	for(i = 0; i < NUM_TEXT_FRAMES_IN_USE; i++)
	{
		gKernelPageTable[i].valid = 1;
		gKernelPageTable[i].prot = PROT_READ|PROT_EXEC;
		gKernelPageTable[i].pfn = i;
	}
	for(i = NUM_TEXT_FRAMES_IN_USE; i < NUM_TEXT_FRAMES_IN_USE + NUM_DATA_FRAMES_IN_USE; i++)
	{
		gKernelPageTable[i].valid = 1;
		gKernelPageTable[i].prot = PROT_READ|PROT_WRITE;
		gKernelPageTable[i].pfn = i;
	}

	// Allocate one page for kernel stack region
	// This has to be in the same spot
	unsigned int stackIndex = KERNEL_STACK_BASE / PAGESIZE;

	// find the first free frame in the available pool
	// move it to the end of the used pool list
	// and assign that frame to this kernel stack page
	FrameTableEntry* avail = gFreeFramePool.m_next;
	if(avail != NULL)
	{
		// set the head of the avail to point to the correct node now
		gFreeFramePool.m_next = avail->m_next;

		// iterate through the used list to paste the avail
		FrameTableEntry* curr = &gUsedFramePool;
		FrameTableEntry* next = curr->m_next;
		while(next != NULL)
		{
			curr = next;
			next = next->m_next;
		}

		// insert the node
		// and reset the pointers
		curr->m_next = avail;
		avail->m_next = NULL;

		// assign this frame number to the pagetable entry
		gKernelPageTable[stackIndex].valid = 1;
		gKernelPageTable[stackIndex].prot = PROT_READ | PROT_WRITE;
		gKernelPageTable[stackIndex].pfn = avail->m_frameNumber;
	}
	
	// set the page table addresses in the registers
	WriteRegister(REG_PTBR0, (unsigned int)gKernelPageTable);
	WriteRegister(REG_PTLR0, gNumPagesR0);
	WriteRegister(REG_PTBR1, (unsigned int)(gKernelPageTable + gNumPagesR0));
	WriteRegister(REG_PTLR1, gNumPagesR1);
	
	// enable virtual memory
	WriteRegister(REG_VM_ENABLE, 1);

	// run idle proces
	// on the top of the VM region - very hacky approach
	// but still works.
	uctx->pc = (void*)DoIdle;
	uctx->sp = (void*)(VMEM_LIMIT - PAGESIZE);
}
