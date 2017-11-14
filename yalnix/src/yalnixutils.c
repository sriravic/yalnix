#include <pagetable.h>
#include <yalnixutils.h>
#include <yalnix.h>

FrameTableEntry* getOneFreeFrame(FrameTableEntry* availPool, FrameTableEntry* usedPool)
{
    // prev cannot be null
    FrameTableEntry* prevA = availPool;
    FrameTableEntry* prevU = usedPool;
    FrameTableEntry* ret = NULL;

    // do one sanity check before proceeding
    if(prevA == NULL || prevU == NULL)
    {
        TracePrintf(0, "Cannot get one free frame as header was NULL\n");

    }

    // pick the first frame
    ret = availPool->m_next;

    // remove this node
    availPool->m_next = ret->m_next;

    // add this node to the last of the used pool
    FrameTableEntry* usedCurr = usedPool;
    FrameTableEntry* usedPrev = usedPool;
    while(usedCurr != NULL)
    {
        usedPrev = usedCurr;
        usedCurr = usedCurr->m_next;
    }

    usedPrev->m_next = ret;
    ret->m_next = NULL;
    return ret;
}

void freeOneFrame(FrameTableEntry* availPool, FrameTableEntry* usedPool, unsigned int frameNum)
{
    FrameTableEntry* prev = usedPool;
    FrameTableEntry* curr = usedPool;
    while(curr != NULL && curr->m_frameNumber != frameNum)
    {
        prev = curr;
        curr = curr->m_next;
    }

    // remove the entry from this list
    prev->m_next = curr->m_next;
    curr->m_next = NULL;

    FrameTableEntry* availCurr = availPool;
    FrameTableEntry* availPrev = availPool;
    while(availCurr != NULL)
    {
        availPrev = availCurr;
        availCurr = availCurr->m_next;
    }

    availPrev->m_next = curr;
}

void freePCB(PCB* pcb)
{
    freeRegionOneFrames(pcb);
    freeKernelStackFrames(pcb);
    exitDataFree(pcb->m_edQ);     // free exit data queue
    free(pcb->m_uctx);
    free(pcb->m_kctx);
    free(pcb->m_pt);
    free(pcb);
}

void freeRegionOneFrames(PCB* pcb)
{
    PageTable* pt = pcb->m_pt;
    int pageNumber;

    // invalidate all the pages for region 1
    // R1 starts from VMEM_1_BASE >> 1 till NUM_VPN
    for(pageNumber = NUM_VPN >> 1; pageNumber < NUM_VPN; pageNumber++)
    {
        if(pt->m_pte[pageNumber].valid == 1)
        {
            freeOneFrame(&gFreeFramePool, &gUsedFramePool, pt->m_pte[pageNumber].pfn);
            pt->m_pte[pageNumber].valid = 0;
        }
    }
}

void freeKernelStackFrames(PCB* pcb)
{
    int pageNumber;
    int r0kernelPages = DOWN_TO_PAGE(KERNEL_STACK_BASE) / PAGESIZE;
    int r0StackPages = DOWN_TO_PAGE(KERNEL_STACK_LIMIT) / PAGESIZE;
    PageTable* pt = pcb->m_pt;

    for(pageNumber = r0kernelPages; pageNumber < r0StackPages; pageNumber++)
    {
        if(pt->m_pte[pageNumber].valid == 1)
        {
            freeOneFrame(&gFreeFramePool, &gUsedFramePool, pt->m_pte[pageNumber].pfn);
            pt->m_pte[pageNumber].valid = 0;
        }
    }
}

// This method swaps out both R1 pages and kernel stack pages
void swapPageTable(PCB* process)
{
    // swap out kernel stack frameSize
    gKernelPageTable.m_pte[gKStackPg0 + 0].pfn = process->m_pagetable->m_kstack[0].pfn;
    gKernelPageTable.m_pte[gKStackPg0 + 1].pfn = process->m_pagetable->m_kstack[1].pfn;

    // swap out R1 space
    WriteRegister(REG_PTBR1, (unsigned int)(process->m_pagetable->m_pte));
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
}

void setR1PageTableAlone(PCB* process)
{
    // swap out R1 space
    WriteRegister(REG_PTBR1, (unsigned int)(process->m_pagetable->m_pte));
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
}
