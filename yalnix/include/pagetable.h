#ifndef __PAGETABLE_H__
#define __PAGETABLE_H__

#include <hardware.h>

typedef struct pte PageTableEntry;

struct fte
{
	unsigned int framenumber : 31;
	unsigned int free	     :  1;
};

typedef struct fte FrameTableEntry;



#endif