/* Team Zoidberg
    A file to implement synchronization data structures and functions
*/

#include "synchronization.h"
#include "yalnix.h"

/***** Lock functions *****/
void lockEnqueue(LockQueueNode* lockQueueNode)
{
    if(gLockQueue.m_head == NULL)
    {
        // empty list
        gLockQueue.m_head = lockQueueNode;
        gLockQueue.m_tail = lockQueueNode;
        lockQueueNode->m_pNext = NULL;
    }
    else
    {
        // add to end
        gLockQueue.m_tail->m_pNext = lockQueueNode;
        lockQueueNode->m_pNext = NULL;
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
        if(currLockNode->m_pLock->m_id == lockId)
        {
            return currLockNode;
        }
        currLockNode = currLockNode->m_pNext;
    }
    return NULL;
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
    newLock->m_state = LOCKED;

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
    newLockQueueNode->m_pLock = newLock;
    newLockQueueNode->m_waitingQueue = newPCBQueue;
    newLockQueueNode->m_pNext = NULL;

    // put the LockQueueNode in the LockQueue
    lockEnqueue(newLockQueueNode);

    // return the new lock's id
    return newLock->m_id;
}

// to be implemented when we write kernelReclaim
int deleteLock(){
    return -1;
}

/***** CVar functions *****/
void cvarEnqueue(CVarQueueNode* cvarQueueNode)
{
    if(gCVarQueue.m_head == NULL)
    {
        // empty list
        gCVarQueue.m_head = cvarQueueNode;
        gCVarQueue.m_tail = cvarQueueNode;
        cvarQueueNode->m_pNext = NULL;
    }
    else
    {
        // add to end
        gCVarQueue.m_tail->m_pNext = cvarQueueNode;
        cvarQueueNode->m_pNext = NULL;
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

CVarQueueNode* getCVarQueueNode(int cvarId)
{
    CVarQueueNode* currCVarQueueNode = gCVarQueue.m_head;
    while(currCVarQueueNode != NULL)
    {
        if(currCVarQueueNode->m_pCVar->m_id == cvarId)
        {
            return currCVarQueueNode;
        }
        currCVarQueueNode = currCVarQueueNode->m_pNext;
    }
    return NULL;
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
    newCVar->m_lockId = 0;

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
    newCVarQueueNode->m_pCVar = newCVar;
    newCVarQueueNode->m_waitingQueue = newPCBQueue;
    newCVarQueueNode->m_pNext = NULL;

    // put the CVarQueueNode in the CVarQueue
    cvarEnqueue(newCVarQueueNode);

    // return the new lock's id
    return newCVar->m_id;
}

int deleteCVar() // to be implemented when we write kernelReclaim
{
    return -1;
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
    else if(type == 2) return SYNC_PIPE;
    else if(type == 3) return SYNC_CVAR;
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
