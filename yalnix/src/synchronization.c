/* Team Zoidberg
    A file to implement synchronization data structures and functions
*/

#include "synchronization.h"

/***** Lock functions *****/
void lockWaitingEnqueue(LockQueueNode* lockNode, PCB* pcb)
{

}

PCB* lockWaitingDequeue(LockQueueNode* lockNode)
{
    return NULL;
}

LockQueueNode* getLockNode(int lockId)
{
    return NULL;
}

int createLock(int pid)
{
    return -1;
}

// to be implemented when we write kernelReclaim
int deleteLock(){
    return -1;
}
