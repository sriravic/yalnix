#ifndef __PAGETABLE_H__
#define __PAGETABLE_H__

#include <hardware.h>

typedef struct pte PageTableEntry;

struct pagetable
{
	PageTableEntry m_pte[NUM_VPN];
};

// some global constants we precompute for ease of DiskAccess
#define gKStackPages	(KERNEL_STACK_MAXSIZE	/ PAGESIZE)
#define gKStackPg0		(KERNEL_STACK_BASE		/ PAGESIZE)
#define gR0Pages		(VMEM_0_SIZE 			/ PAGESIZE)
#define gR1Pages		(VMEM_1_SIZE			/ PAGESIZE)

// This is the global Kernel page table which is shared by all processes
struct KernelPageTable
{
	PageTableEntry m_pte[gR0Pages];
};

// The user mode page tables consist of R1 pages and its corresponding stack frames
struct UserProgPageTable
{
	PageTableEntry m_pte[gR1Pages];
	PageTableEntry m_kstack[gKStackPages];
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

#endif
