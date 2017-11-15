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
        return NULL;
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

FrameTableEntry* getNFreeFrames(FrameTableEntry* availPool, FrameTableEntry* usedPool, int nframes)
{
    if(nframes == 1) return getOneFreeFrame(availPool, usedPool);
    else
    {
        // prev cannot be null
        FrameTableEntry* prevA = availPool;
        FrameTableEntry* prevU = usedPool;
        FrameTableEntry* ret = NULL;

        // do one sanity check before proceeding
        if(prevA == NULL || prevU == NULL)
        {
            TracePrintf(0, "Cannot get one free frame as header was NULL\n");
            return NULL;
        }

        // pick the contiguous chunk of frames
        ret = availPool->m_next;
        FrameTableEntry* temp = ret;
        int i;
        for(i = 1; i < nframes; i++)
        {
            temp = temp->m_next;
            if(temp == NULL)
            {
                // we cannot find one large chunk
                // return null silently
                return NULL;
            }
        }

        availPool->m_next = temp->m_next;

        // add the contiguous chunk of elements to the end of the used list
        FrameTableEntry* usedCurr = usedPool;
        FrameTableEntry* usedPrev = usedPool;
        while(usedCurr != NULL)
        {
            usedPrev = usedCurr;
            usedCurr = usedCurr->m_next;
        }

        usedPrev->m_next = ret;
        temp->m_next = NULL;
        return ret;
    }
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
    SAFE_FREE(pcb->m_uctx);
    SAFE_FREE(pcb->m_kctx);
    SAFE_FREE(pcb->m_pagetable);
    SAFE_FREE(pcb->m_pt);
    SAFE_FREE(pcb);
}

void freeRegionOneFrames(PCB* pcb)
{
    UserProgPageTable* pt = pcb->m_pagetable;
    int pageNumber;

    // invalidate all the pages for region 1
    // R1 starts from VMEM_1_BASE >> 1 till NUM_VPN
    for(pageNumber = 0; pageNumber < gNumPagesR1; pageNumber++)
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
    UserProgPageTable* pt = pcb->m_pagetable;

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
    gKernelPageTable.m_pte[KSTACK_PAGE0 + 0].pfn = process->m_pagetable->m_kstack[0].pfn;
    gKernelPageTable.m_pte[KSTACK_PAGE0 + 1].pfn = process->m_pagetable->m_kstack[1].pfn;

    // swap out R1 space
    WriteRegister(REG_PTBR1, (unsigned int)(process->m_pagetable->m_pte));
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
}

void setR1PageTableAlone(PCB* process)
{
    // swap out R1 space
    WriteRegister(REG_PTBR1, (unsigned int)(process->m_pagetable->m_pte));
    WriteRegister(REG_PTLR1, R1PAGES);
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
}

// Returns -1 in case  of ERROR
// Returns 0 in case of success
// NOTE: What about stack space that has grown down and then grown up?
//       Such pages are still valid.!
int checkValidAddress(unsigned int addr, PCB* pcb)
{
    // check for any address is region0
    if(addr < VMEM_1_BASE) return -1;

    // check if address is in a valid region in R1
    // that includes, stack space, and only heap space
    // text and data are WRITE_PROTECTED.
    // so we can just AND and see if our bit is set.
    int r1page = addr / PAGESIZE;
    r1page -= gNumPagesR0;

    UserProgPageTable* currpt = pcb->m_pagetable;
    if(currpt->m_pte[r1page].valid == 0 || (currpt->m_pte[r1page].prot & PROT_WRITE) == 0) return -1;
    else return 0;
}
