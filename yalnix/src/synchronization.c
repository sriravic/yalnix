/* Team Zoidberg
    A file to implement synchronization data structures and functions
*/

#include "synchronization.h"
#include "yalnix.h"
#include "yalnixutils.h"

/***** Lock functions *****/
void lockNodeEnqueue(LockQueueNode* lockQueueNode)
{
    if(gLockQueue.m_head == NULL)
    {
        // empty list
        gLockQueue.m_head = lockQueueNode;
        gLockQueue.m_tail = lockQueueNode;
        lockQueueNode->m_next = NULL;
    }
    else
    {
        // add to end
        gLockQueue.m_tail->m_next = lockQueueNode;
        lockQueueNode->m_next = NULL;
        gLockQueue.m_tail = lockQueueNode;
    }
}

void lockWaitingEnqueue(LockQueueNode* lockNode, PCB* pcb)
{
    processEnqueue(lockNode->m_waitingQueue, pcb);
}

PCB* lockWaitingDequeue(LockQueueNode* lockNode)
{
    return processDequeue(lockNode->m_waitingQueue);
}

LockQueueNode* getLockNode(int lockId)
{
    LockQueueNode* currLockNode = gLockQueue.m_head;
    while(currLockNode != NULL)
    {
        if(currLockNode->m_lock->m_id == lockId)
        {
            return currLockNode;
        }
        currLockNode = currLockNode->m_next;
    }
    return NULL;
}

int removeLockNode(LockQueueNode* lockNode)
{
    if(lockNode == gLockQueue.m_head && lockNode == gLockQueue.m_tail)
    {
        // removing the only item in the list
        gLockQueue.m_head = NULL;
        gLockQueue.m_tail = NULL;
        lockNode->m_next = NULL;
        return SUCCESS;
    }
    else if(lockNode == gLockQueue.m_head)
    {
        // removing the head
        gLockQueue.m_head = lockNode->m_next;
        lockNode->m_next = NULL;
        return SUCCESS;
    }
    else
    {
        // normal case
        LockQueueNode* currNode = gLockQueue.m_head;
        while(currNode->m_next != NULL)
        {
            if(currNode->m_next == lockNode)
            {
                // patch up the LL and return
                currNode->m_next = currNode->m_next->m_next;
                lockNode->m_next = NULL;
                if(lockNode == gLockQueue.m_tail)
                {
                    gLockQueue.m_tail = currNode;
                }
                return SUCCESS;
            }
            currNode = currNode->m_next;
        }
    }
    // not found
    return ERROR;
}

int createLock(int pid)
{
    // initialize new lock
    Lock* newLock = (Lock*)malloc(sizeof(Lock));
    if(newLock == NULL)
    {
        return ERROR;
    }
    newLock->m_id = getUniqueSyncId(SYNC_LOCK);
    newLock->m_owner = pid;
    newLock->m_state = UNLOCKED;

    // initialize new PCBQueue for the waiting list
    PCBQueue* newPCBQueue = (PCBQueue*)malloc(sizeof(PCBQueue));
    if(newPCBQueue == NULL)
    {
        return ERROR;
    }
    memset(newPCBQueue, 0, sizeof(PCBQueue));

    // initialize new LockQueueNode
    LockQueueNode* newLockQueueNode = (LockQueueNode*)malloc(sizeof(LockQueueNode));
    if(newLockQueueNode == NULL)
    {
        return ERROR;
    }
    newLockQueueNode->m_lock = newLock;
    newLockQueueNode->m_waitingQueue = newPCBQueue;
    newLockQueueNode->m_holder = -1;
    newLockQueueNode->m_next = NULL;

    // put the LockQueueNode in the LockQueue
    lockNodeEnqueue(newLockQueueNode);

    // return the new lock's id
    return newLock->m_id;
}

// to be implemented when we write kernelReclaim
int freeLock(LockQueueNode* lockNode){
    if(lockNode->m_waitingQueue->m_head != NULL)
    {
        // still processes waiting so return Error
        return ERROR;
    }
    removeLockNode(lockNode);
    SAFE_FREE(lockNode->m_waitingQueue);
    SAFE_FREE(lockNode->m_lock);
    SAFE_FREE(lockNode);
    return SUCCESS;
}

/***** CVar functions *****/
void cvarNodeEnqueue(CVarQueueNode* cvarQueueNode)
{
    if(gCVarQueue.m_head == NULL)
    {
        // empty list
        gCVarQueue.m_head = cvarQueueNode;
        gCVarQueue.m_tail = cvarQueueNode;
        cvarQueueNode->m_next = NULL;
    }
    else
    {
        // add to end
        gCVarQueue.m_tail->m_next = cvarQueueNode;
        cvarQueueNode->m_next = NULL;
        gCVarQueue.m_tail = cvarQueueNode;
    }
}

void cvarWaitingEnqueue(CVarQueueNode* cvarQueueNode, PCB* pcb)
{
    processEnqueue(cvarQueueNode->m_waitingQueue, pcb);
}

PCB* cvarWaitingDequeue(CVarQueueNode* cvarQueueNode)
{
    return processDequeue(cvarQueueNode->m_waitingQueue);
}

CVarQueueNode* getCVarNode(int cvarId)
{
    CVarQueueNode* currCVarQueueNode = gCVarQueue.m_head;
    while(currCVarQueueNode != NULL)
    {
        if(currCVarQueueNode->m_cvar->m_id == cvarId)
        {
            return currCVarQueueNode;
        }
        currCVarQueueNode = currCVarQueueNode->m_next;
    }
    return NULL;
}

int removeCVarNode(CVarQueueNode* cvarNode)
{
    if(cvarNode == gCVarQueue.m_head && cvarNode == gCVarQueue.m_tail)
    {
        // removing the only item in the list
        gCVarQueue.m_head = NULL;
        gCVarQueue.m_tail = NULL;
        cvarNode->m_next = NULL;
        return SUCCESS;
    }
    else if(cvarNode == gCVarQueue.m_head)
    {
        // removing the head
        gCVarQueue.m_head = cvarNode->m_next;
        cvarNode->m_next = NULL;
        return SUCCESS;
    }
    else
    {
        // normal case
        CVarQueueNode* currNode = gCVarQueue.m_head;
        while(currNode->m_next != NULL)
        {
            if(currNode->m_next == cvarNode)
            {
                // patch up the LL and return
                currNode->m_next = currNode->m_next->m_next;
                cvarNode->m_next = NULL;
                if(cvarNode == gCVarQueue.m_tail)
                {
                    gCVarQueue.m_tail = currNode;
                }
                return SUCCESS;
            }
            currNode = currNode->m_next;
        }
    }
    // not found
    return ERROR;
}

int createCVar(int pid)
{
    // initialize new cvar
    CVar* newCVar = (CVar*)malloc(sizeof(CVar));
    if(newCVar == NULL)
    {
        return ERROR;
    }
    newCVar->m_id = getUniqueSyncId(SYNC_CVAR);
    newCVar->m_owner = pid;
    newCVar->m_lockId = -1;

    // initialize new PCBQueue for the waiting list
    PCBQueue* newPCBQueue = (PCBQueue*)malloc(sizeof(PCBQueue));
    if(newPCBQueue == NULL)
    {
        return ERROR;
    }
    memset(newPCBQueue, 0, sizeof(PCBQueue));

    // initialize new CVarQueueNode
    CVarQueueNode* newCVarQueueNode = (CVarQueueNode*)malloc(sizeof(CVarQueueNode));
    if(newCVarQueueNode == NULL)
    {
        return ERROR;
    }
    newCVarQueueNode->m_cvar = newCVar;
    newCVarQueueNode->m_waitingQueue = newPCBQueue;
    newCVarQueueNode->m_next = NULL;

    // put the CVarQueueNode in the CVarQueue
    cvarNodeEnqueue(newCVarQueueNode);

    // return the new lock's id
    return newCVar->m_id;
}

int freeCVar(CVarQueueNode* cvarNode) // to be implemented when we write kernelReclaim
{
    if(cvarNode->m_waitingQueue->m_head != NULL)
    {
        // still processes waiting so return Error
        return ERROR;
    }
    removeCVarNode(cvarNode);
    SAFE_FREE(cvarNode->m_waitingQueue);
    SAFE_FREE(cvarNode->m_cvar);
    SAFE_FREE(cvarNode);
    return SUCCESS;
}

/***** utility functions *****/
int getUniqueSyncId(SyncType t)
{
    int nextId = gSID++;
    if(t == SYNC_LOCK)
        return (nextId | LOCK_MASK);
    else if(t == SYNC_CVAR)
        return (nextId | CVAR_MASK);
    else if(t == SYNC_PIPE)
        return (nextId | PIPE_MASK);
    else
    {
        TracePrintf(0, "INVALID SyncType passed.!!");
        return 0xFFFFFFFF;
    }
}

SyncType getSyncType(int compoundId)
{
    int type = (compoundId & 0x30000000) >> SYNC_SHIFT;
    if(type == 1) return SYNC_LOCK;
    else if(type == 2) return SYNC_CVAR;
    else if(type == 3) return SYNC_PIPE;
    else
    {
        TracePrintf(0, "ERROR: Invalid Sync Type\n");
        return SYNC_UNDEFINED;
    }
}

// strips the sync type mask.
int getSyncIdOnly(int compoundId)
{
    return (compoundId & 0x0FFFFFFF);
}

// adds a new entry in the global pipe lists
void pipeEnqueue(int uid)
{
    PipeQueueNode* node = (PipeQueueNode*)malloc(sizeof(PipeQueueNode));
    node->m_pipe = (Pipe*)malloc(sizeof(Pipe));
    if(node->m_pipe != NULL)
    {
        node->m_pipe->m_len = 0;
        node->m_pipe->m_validLength = 0;
        node->m_pipe->m_buffer = NULL;
        node->m_pipe->m_id = uid;
    }
    else
    {
        TracePrintf(0, "Error allocating space for pipe entry\n");
    }

    // do queue operations
    if(gPipeQueue.m_head == NULL)
    {
        // empty list
        gPipeQueue.m_head = node;
        gPipeQueue.m_tail = node;
        node->m_next = NULL;
    }
    else
    {
        // add to end
        gPipeQueue.m_tail->m_next = node;
        node->m_next = NULL;
        gPipeQueue.m_tail = node;
    }

}

PipeQueueNode* getPipeNode(int pipeId)
{
    PipeQueueNode* currPipeQueueNode = gPipeQueue.m_head;
    while(currPipeQueueNode != NULL)
    {
        if(currPipeQueueNode->m_pipe->m_id == pipeId)
        {
            return currPipeQueueNode;
        }
        currPipeQueueNode = currPipeQueueNode->m_next;
    }
    return NULL;
}

int pipeReadWaitEnqueue(int id, int len, PCB* pcb, void* buff)
{
    PipeReadWaitQueueNode* node = (PipeReadWaitQueueNode*)malloc(sizeof(PipeReadWaitQueueNode));
    if(node != NULL)
    {
        node->m_pcb = pcb;
        node->m_id = id;
        node->m_len = len;
        node->m_buf = buff;
        if(gPipeReadWaitQueue.m_head == NULL)
        {
            gPipeReadWaitQueue.m_head = node;
            gPipeReadWaitQueue.m_tail = node;
            node->m_next = NULL;
        }
        else
        {
            gPipeReadWaitQueue.m_tail->m_next = node;
            node->m_next = NULL;
            gPipeReadWaitQueue.m_tail = node;
        }
    }
    else
    {
        TracePrintf(0, "Error allocating memory for pipe read wait queue node\n");
        return ERROR;
    }
}

void processPendingPipeReadRequests()
{
    PCB* currpcb = getHeadProcess(&gRunningProcessQ);
    PipeReadWaitQueueNode* node = gPipeReadWaitQueue.m_head;
    if(node != NULL)
    {
        do {
            int pipeid = node->m_id;
            PipeQueueNode* pipeNode = getPipeNode(pipeid);
            if(pipeNode != NULL)
            {
                Pipe* pipe = pipeNode->m_pipe;
                int requested = node->m_len;
                int available = pipe->m_validLength;
                if(requested <= available)
                {
                    // we have the requested bytes
                    // temporarily swap the page pagetables
                    // and copy the contents into the processes address space
                    swapPageTable(node->m_pcb);
                    memcpy(node->m_buf, pipe->m_buffer, sizeof(requested));
                }
                else
                {
                    // We did not get enough
                    // break and try again later
                    break;
                }
            }
            else
            {
                // pipe was not found
                // goto next nodes
                node = node->m_next;
            }
        } while(node != NULL);
    }
}

int removePipeNode(PipeQueueNode* pipeNode)
{
    if(pipeNode == gPipeQueue.m_head && pipeNode == gPipeQueue.m_tail)
    {
        // removing the only item in the list
        gPipeQueue.m_head = NULL;
        gPipeQueue.m_tail = NULL;
        pipeNode->m_next = NULL;
        return SUCCESS;
    }
    else if(pipeNode == gPipeQueue.m_head)
    {
        // removing the head
        gPipeQueue.m_head = pipeNode->m_next;
        pipeNode->m_next = NULL;
        return SUCCESS;
    }
    else
    {
        // normal case
        PipeQueueNode* currNode = gPipeQueue.m_head;
        while(currNode->m_next != NULL)
        {
            if(currNode->m_next == pipeNode)
            {
                // patch up the LL and return
                currNode->m_next = currNode->m_next->m_next;
                pipeNode->m_next = NULL;
                if(pipeNode == gPipeQueue.m_tail)
                {
                    gPipeQueue.m_tail = currNode;
                }
                return SUCCESS;
            }
            currNode = currNode->m_next;
        }
    }
    // not found
    return ERROR;
}

int freePipe(PipeQueueNode* pipeNode)
{
    removePipeNode(pipeNode);
    SAFE_FREE(pipeNode->m_pipe->m_buffer);
    SAFE_FREE(pipeNode->m_pipe);
    SAFE_FREE(pipeNode);
    return SUCCESS;
}
