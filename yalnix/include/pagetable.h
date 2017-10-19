#ifndef __PAGETABLE_H__
#define __PAGETABLE_H__

#include <hardware.h>

typedef struct pte PageTableEntry;

// this structure is used by the kernel to allocate free frames for the page tables
struct fte
{
	unsigned int m_frameNumber : 31;		// we can easily compute the base address (frameNumber * frameSize)
	unsigned int m_head		   :  1;		// 1 bit to indicate if its head pointer
	struct fte* m_next;
};

typedef struct fte FrameTableEntry;

#endif