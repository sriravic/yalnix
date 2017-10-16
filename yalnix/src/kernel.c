#include <yalnix.h>
#include <filesystem.h>
#include <hardware.h>
#include <load_info.h>
#include <interrupt_handler.h>

void* globalBrk;

typedef struct pte PageTableEntry;
unsigned int gNumPagesR0;		// region0 pages
unsigned int gNumPagesR1;		// region1 pages

// page table address
unsigned int gR0PtBegin;
unsigned int gR0PtEnd;
unsigned int gR1PtBegin;
unsigned int gR1PtEnd;

// kernel text and data
unsigned int gKernelDataStart;
unsigned int gKernelDataEnd;

// interrupt vector table
// we have 7 types of interrupts
void (*gIVT[TRAP_VECTOR_SIZE])(UserContext*);

int gVMemEnabled = -1;			// global flag to keep track of the enabling of virtual memory

PageTableEntry* gPageTable;	// initial page tables

unsigned int getKB(unsigned int size) { return size / (1 << 10); }
unsigned int getMB(unsigned int size) { return size / (1 << 20); }
unsigned int getGB(unsigned int size) { return size / (1 << 30); }

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
	gIVT[0] = (void*)&interruptKernel;
	gIVT[1] = (void*)&interruptClock;
	gIVT[2] = (void*)&interruptIllegal;
	gIVT[3] = (void*)&interruptMemory;
	gIVT[4] = (void*)&interruptMath;
	gIVT[5] = (void*)&interruptTtyReceive;
	gIVT[6] = (void*)&interruptTtyTransmit;
	int i;
	for(i = 7; i < TRAP_VECTOR_SIZE; i++)
		gIVT[i] = (void*)&interruptDummy;
	
	unsigned int ivtBaseRegAddr = (unsigned int)(&(gIVT[0]));
	TracePrintf(0, "Base IVT Register address : 0x%08X\n", ivtBaseRegAddr);
	WriteRegister(REG_VECTOR_BASE, ivtBaseRegAddr);
	
	// create the initial page tables
	unsigned int TOTAL_FRAMES = pmem_size / PAGESIZE;		// compute the total number of frames
	unsigned int TOTAL_PAGES  = VMEM_SIZE / PAGESIZE;		// compute the total number of pages
	unsigned int NUM_R0_PAGES = VMEM_0_SIZE / PAGESIZE;	// the total number of pages required for r0
	unsigned int NUM_R1_PAGES = VMEM_1_SIZE / PAGESIZE;	// the total number of pages required for r1
	TracePrintf(0, "total frames : %u\n", TOTAL_FRAMES);
	TracePrintf(0, "total pages  : %u\n", TOTAL_PAGES);
	TracePrintf(0, "total R0 pages : %u\n", NUM_R0_PAGES);
	TracePrintf(0, "total R1 pages : %u\n", NUM_R1_PAGES);
	
	unsigned int dataStart = (unsigned int)gKernelDataStart;
	unsigned int dataEnd = (unsigned int)gKernelDataEnd;
	unsigned int dataStartRounded = UP_TO_PAGE(dataStart);
	unsigned int dataEndRounded = UP_TO_PAGE(dataEnd);
	unsigned int textEnd = DOWN_TO_PAGE(dataStart);
	TracePrintf(0, "Data Start rounded : 0x%08X\n", dataStartRounded);
	TracePrintf(0, "Data End Rounded : 0x%08X\n", dataEndRounded);
	
	unsigned int NUM_TEXT_FRAMES = textEnd / PAGESIZE;
	unsigned int NUM_DATA_FRAMES = (dataEndRounded - dataStartRounded) / PAGESIZE;
	unsigned int NUM_R0_FRAMES = TOTAL_FRAMES - 2; 	// lets just have two frames left for region 1			
	unsigned int NUM_R1_FRAMES = NUM_R0_PAGES;					// lets just have one pa
	
	TracePrintf(0, "Data end rounded in decimal : %d\n", dataEndRounded);
	TracePrintf(0, "Page size in decimal : %d\n", PAGESIZE);
	TracePrintf(0, "Num physical frames : %u\n", TOTAL_FRAMES);
	TracePrintf(0, "Num Text pages : %u\n", NUM_TEXT_FRAMES);
	TracePrintf(0, "Num Data pages : %u\n", NUM_DATA_FRAMES);
	
	gPageTable = (PageTableEntry*)malloc(sizeof(PageTableEntry) * TOTAL_PAGES);
	if(gPageTable != NULL)
	{
		int i;
		for(i = 0; i < NUM_TEXT_FRAMES; i++)
		{
			gPageTable[i].valid = 1;
			gPageTable[i].prot = PROT_READ|PROT_EXEC;
			gPageTable[i].pfn = i;
		}
		for(i = NUM_TEXT_FRAMES; i < TOTAL_PAGES; i++)
		{
			gPageTable[i].valid = 1;
			gPageTable[i].prot = PROT_READ|PROT_WRITE;
			gPageTable[i].pfn = i;
		}
	}
	else
	{
		TracePrintf(0, "error creating initial page tables\n");		
	}
	
	// set the limit register values
	gR0PtBegin = (unsigned int)gPageTable;
	gR1PtBegin = (unsigned int)(gPageTable + NUM_R0_PAGES);
	gNumPagesR0 = NUM_R0_PAGES;
	gNumPagesR1 = NUM_R1_PAGES;
	
	// set the page table addresses in the registers
	WriteRegister(REG_PTBR0, gR0PtBegin);
	WriteRegister(REG_PTLR0, gNumPagesR0);
	WriteRegister(REG_PTBR1, gR1PtBegin);
	WriteRegister(REG_PTLR1, gNumPagesR1);
	
	// enable virtual memory
	WriteRegister(REG_VM_ENABLE, 1);
}
