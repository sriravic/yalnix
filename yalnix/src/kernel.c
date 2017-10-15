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

// interrupt vector table
// we have 7 types of interrupts
void (*gIVT[TRAP_VECTOR_SIZE])(UserContext*);

int gVMemEnabled = -1;			// global flag to keep track of the enabling of virtual memory

PageTableEntry* gPageTable;

unsigned int getKB(unsigned int size) { return size / (1 << 10); }
unsigned int getMB(unsigned int size) { return size / (1 << 20); }
unsigned int getGB(unsigned int size) { return size / (1 << 30); }

int SetKernelBrk(void* addr)
{
	if(gVMemEnabled == -1)
	{
		// virtual memory has not yet been set.
		printf("SetKernelBrk : 0x%08X\n", addr);
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
    printf("DataStart  : 0x%08X\n", _KernelDataStart);
    printf("DataEnd    : 0x%08X\n", _KernelDataEnd);

	// create the initial page tables
	unsigned int dataStart = (unsigned int)_KernelDataStart;
	unsigned int dataEnd = (unsigned int)_KernelDataEnd;
	unsigned int dataStartRounded = UP_TO_PAGE(dataStart);
	unsigned int dataEndRounded = UP_TO_PAGE(dataEnd);
	printf("Data Start rounded : 0x%08X\n", dataStartRounded);
	printf("Data End Rounded : 0x%08X\n", dataEndRounded);
	
	unsigned int NUM_EXEC_PAGES = dataStartRounded / PAGESIZE;
	unsigned int NUM_DATA_PAGES = (dataEnd - dataStartRounded) / PAGESIZE;
	unsigned int NUM_PAGES = dataEndRounded / PAGESIZE;
	printf("Num physical pages : %u\n", NUM_PAGES);
	
	gPageTable = (PageTableEntry*)malloc(sizeof(PageTableEntry) * NUM_PAGES);
	if(gPageTable != NULL)
	{
		gNumPagesR0 = NUM_PAGES;
		int i;
		for(i = 0; i < NUM_EXEC_PAGES; i++)
		{
			gPageTable[i].valid = 1;
			gPageTable[i].prot = PROT_READ | PROT_EXEC;
			gPageTable[i].pfn = i;
		}
		for(i = NUM_EXEC_PAGES; i < NUM_DATA_PAGES; i++)
		{
			gPageTable[i].valid = 1;
			gPageTable[i].prot = PROT_READ | PROT_WRITE;
			gPageTable[i].pfn = i;
		}
	}
	else
	{
		printf("error creating initial page tables\n");		
	}
	
	// set the limit register values
	gR0PtBegin = (unsigned int)&gPageTable;
}


void DoIdle(void)
{
	while(1)
	{
		TracePrintf(1, "DoIdle\n");
		Pause();
	}
}

void KernelStart(char** argv, unsigned int pmem_size, UserContext* uctx)
{
    printf("KernelStart Function\n");
    
    // parse the argvs
    int argc = 0;
    while(argv[argc] != NULL)
    {
        printf("\t Argv : %s\n", argv[argc++]);
    }

    printf("Available memory : %u MB\n", getMB(pmem_size));
	
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
	printf("Base IVT Register address : 0x%08X\n", ivtBaseRegAddr);
	WriteRegister(REG_VECTOR_BASE, ivtBaseRegAddr);
	
	// create all the data structures required
	
	// set the page table addresses in the registers
	WriteRegister(REG_PTBR0, gR0PtBegin);
	WriteRegister(REG_PTLR0, gNumPagesR0);
	
	// enable virtual memory
	WriteRegister(REG_VM_ENABLE, 1);
	
	// call do idle
	DoIdle();
}
