#include <yalnix.h>
#include <filesystem.h>
#include <hardware.h>
#include <load_info.h>
#include <pagetable.h>

void* globalBrk;

typedef struct pte PageTableEntry;
unsigned int gNumPagesR0;		// region0 pages
unsigned int gNumPagesR1;		// region1 pages

PageTableEntry* gPageTable;

unsigned int getKB(unsigned int size) { return size / (1 << 10); }
unsigned int getMB(unsigned int size) { return size / (1 << 20); }
unsigned int getGB(unsigned int size) { return size / (1 << 30); }

int SetKernelBrk(void* addr)
{
    printf("SetKernelBrk : 0x%08X\n", addr);
    globalBrk = addr;
    return 0;
}

void SetKernelData(void* _KernelDataStart, void* _KernelDataEnd)
{
    globalBrk = _KernelDataEnd;
    printf("DataStart  : 0x%08X\n", _KernelDataStart);
    printf("DataEnd    : 0x%08X\n", _KernelDataEnd);

	// create the initial page tables
	unsigned int dataEnd = (unsigned int)_KernelDataEnd;
	unsigned int dataEndRounded = UP_TO_PAGE(dataEnd);
	printf("Data End Rounded : 0x%08X\n", dataEndRounded);
	
	unsigned int NUM_PAGES = dataEndRounded / PAGE_SIZE;
	printf("Num physical pages : %u\n", NUM_PAGES);
	
	gPageTable = (PageTableEntry*)malloc(sizeof(PageTableEntry) * NUM_PAGES);
	if(gPageTable != NULL)
	{
		gNumPagesR0 = NUM_PAGES;
		for(int i = 0; i < NUM_PAGES; i++)
		{
			gPageTable[i].valid = 1;
			gPageTable[i].prot = PROT_READ | PROT_EXEC;
			gPageTable[i].pfn = i;
		}
	}
	else
	{
		printf("error creating initial page tables\n");		
	}
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
}
