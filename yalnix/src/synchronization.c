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

// Utility functions
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

Pipe* getPipeNode(int uid)
{
    PipeQueueNode* curr = gPipeQueue.m_head;
    PipeQueueNode* next = curr;
    if(curr == NULL)
    {
        TracePrintf(0, "ERROR: PipeQueue was empty\n");
    }
    while(next != NULL)
    {
        if(curr->m_pipe->m_id == uid) return curr->m_pipe;
        else
        {
            curr = next;
            next = next->m_next;
        }
    }
    TracePrintf(0, "Pipe ID was not found\n");
    return NULL;
}

int pipeReadWaitEnqueue(int id, int len, PCB* pcb)
{
    PipeReadWaitQueueNode* node = (PipeReadWaitQueueNode*)malloc(sizeof(PipeReadWaitQueueNode));
    if(node != NULL)
    {
        node->m_pcb = pcb;
        node->m_id = id;
        node->m_len = len;
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

}

void freePipe(int id)
{
    
}
