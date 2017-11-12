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
    newLock->m_id = gSID;
    gSID++;
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
