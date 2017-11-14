#ifndef __YALNIX_UTILS_H__
#define __YALNIX_UTILS_H__

#include <pagetable.h>
#include "process.h"

// utility functions that we add to make our life easier within the yalnix environemtn
static inline unsigned int getKB(unsigned int size) { return size >> 10; }
static inline unsigned int getMB(unsigned int size) { return size >> 20; }
static inline unsigned int getGB(unsigned int size) { return size >> 30; }

// Fetches the first available free frame entry given the header
// pointers to the avail and used pool
// does the book keeping of moving the returned frame to the used pool
FrameTableEntry* getOneFreeFrame(FrameTableEntry* availPool, FrameTableEntry* usedPool);

// finds the node for framenum and removes it from the used pool and moves it to the free frames
void freeOneFrame(FrameTableEntry* availPool, FrameTableEntry* usedPool, unsigned int frameNum);

// free all memory associated with a pcb
void freePCB(PCB* pcb);

// frees all region one frames associated with the given pcb
void freeRegionOneFrames(PCB* pcb);

// Free the two kernel stack frames associated with the pcb
void freeKernelStackFrames(PCB* pcb);

// swaps the page table to the PCB passed in
void swapPageTable(PCB* process);

// set the R1 region alone. don't swap out the kernel stack.
void setR1PageTableAlone(PCB* process);

#endif
