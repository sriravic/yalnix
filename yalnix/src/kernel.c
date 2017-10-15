#include <yalnix.h>
#include <filesystem.h>
#include <hardware.h>
#include <load_info.h>
#include <pagetable.h>

void* globalBrk;

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

    unsigned int startAddress = 0;
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
