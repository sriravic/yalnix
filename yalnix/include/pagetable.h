#ifndef __PAGETABLE_H__
#define __PAGETABLE_H__

#include <hardware.h>

typedef struct pte PageTableEntry;

struct pagetable
{
	PageTableEntry m_pte[NUM_VPN];
};

// some global constants we precompute for ease of DiskAccess
#define KSTACK_PAGES	(KERNEL_STACK_MAXSIZE	/ PAGESIZE)
#define KSTACK_PAGE0	(KERNEL_STACK_BASE		/ PAGESIZE)
#define R0PAGES			(VMEM_0_SIZE 			/ PAGESIZE)
#define R1PAGES			(VMEM_1_SIZE			/ PAGESIZE)

extern unsigned int gNumPagesR0;
extern unsigned int gNumPagesR1;
extern unsigned int gKStackPages;
extern unsigned int gKStackPg0;

// This is the global Kernel page table which is shared by all processes
struct KernelPageTable
{
	PageTableEntry m_pte[NUM_VPN];
};

// The user mode page tables consist of R1 pages and its corresponding stack frames
struct UserProgPageTable
{
	PageTableEntry m_pte[R1PAGES];
	PageTableEntry m_kstack[KSTACK_PAGES];
};

typedef struct KernelPageTable KernelPageTable;
typedef struct UserProgPageTable UserProgPageTable;

typedef struct pagetable PageTable;

// this structure is used by the kernel to allocate free frames for the page tables
struct fte
{
	unsigned int m_frameNumber : 31;		// we can easily compute the base address (frameNumber * frameSize)
	unsigned int m_head		   :  1;		// 1 bit to indicate if its head pointer
	struct fte* m_next;
};

typedef struct fte FrameTableEntry;

extern FrameTableEntry gFreeFramePool;
extern FrameTableEntry gUsedFramePool;
extern KernelPageTable gKernelPageTable;
extern UserProgPageTable* gCurrentR1PageTable;

#endif
