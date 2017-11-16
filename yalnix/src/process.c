/* Team Zoidberg
    A file to implement a queue of PCB data structures as a linked list
*/

#include <stdbool.h>
#include <process.h>
#include <yalnixutils.h>
#include <synchronization.h>

PCB* processDequeue(PCBQueue* Q)
{
    if (Q->m_head == NULL) {
        // Empty queue
        return NULL;
    }
    else if (Q->m_head == Q->m_tail) {
        // Single item in queue
        PCB* pcb = Q->m_head;
        Q->m_head = NULL;
        Q->m_tail = NULL;
        Q->m_size--;
        pcb->m_next = NULL;
        pcb->m_prev = NULL;
        return pcb;
    } else {
        // remove from beginning
        PCB* pcb = Q->m_head;
        Q->m_head = pcb->m_next;
        Q->m_size--;
        pcb->m_next = NULL;
        pcb->m_prev = NULL;
        return pcb;
    }
}

void processEnqueue(PCBQueue* Q, PCB* process)
{
    if (Q->m_head == NULL) {
        // empty list
        Q->m_head = process;
        Q->m_tail = process;
        process->m_next = NULL;
        process->m_prev = NULL;
    }
    else {
        // add to end
        Q->m_tail->m_next = process;
        process->m_next = NULL;
        process->m_prev = Q->m_tail;
        Q->m_tail = process;
    }
    Q->m_size++;
}

/*  Method to remove a process from the given queue in O(1) time
    Needs the PCBQueue in case it needs to update the head or tail
    Note- it falls to the user to ensure that the process is actually in the queue in the first place
 */
void processRemove(PCBQueue* Q, PCB* process)
{
    if(Q->m_head == process)
    {
        // simply dequeue (takes care of process=head=tail also)
        processDequeue(Q);
        return;
    }
    else if(Q->m_tail == process)
    {
        // simply remove from the end of the queue (separate bc need to update the queue's tail)
        Q->m_tail = process->m_prev;
        Q->m_tail->m_next = NULL;
        process->m_next = NULL;
        process->m_prev = NULL;
    }
    else
    {
        // normal case- remove from middle of the list
        process->m_prev->m_next = process->m_next;
        process->m_next->m_prev = process->m_prev;
        process->m_next = NULL;
        process->m_prev = NULL;
    }
    Q->m_size--;
}

// return the PCB with pid in the given queue, or NULL if there is not one
PCB* getPcbByPid(PCBQueue* Q, int pid)
{
    PCB* curr = Q->m_head;
    while(curr != NULL)
    {
        if(curr->m_pid == pid)
        {
            return curr;
        }
        else
        {
            curr = curr->m_next;
        }
    }
    return NULL;
}

PCB* getPcbByPidInLocks(int pid)
{
    LockQueueNode* lockNode = gLockQueue.m_head;
    PCB* pcb = NULL;
    while(lockNode != NULL)
    {
        pcb = getPcbByPid(lockNode->m_waitingQueue, pid);
        if(pcb != NULL)
        {
            return pcb;
        }
        lockNode = lockNode->m_next;
    }
    return NULL;
}

PCB* getPcbByPidInCVars(int pid)
{
    CVarQueueNode* cvarNode = gCVarQueue.m_head;
    PCB* pcb = NULL;
    while(cvarNode != NULL)
    {
        pcb = getPcbByPid(cvarNode->m_waitingQueue, pid);
        if(pcb != NULL)
        {
            return pcb;
        }
        cvarNode = cvarNode->m_next;
    }
    return NULL;
}

// return the first PCB that is a child of the given ppid, or NULL if there is not one
PCB* getChildOfPpid(PCBQueue* Q, int ppid)
{
    PCB* curr = Q->m_head;
    while(curr != NULL)
    {
        if(curr->m_ppid == ppid)
        {
            return curr;
        }
        else
        {
            curr = curr->m_next;
        }
    }
    return NULL;
}

PCB* getChildOfPpidInLocks(int ppid)
{
    LockQueueNode* lockNode = gLockQueue.m_head;
    PCB* pcb = NULL;
    while(lockNode != NULL)
    {
        pcb = getChildOfPpid(lockNode->m_waitingQueue, ppid);
        if(pcb != NULL)
        {
            return pcb;
        }
        lockNode = lockNode->m_next;
    }
    return NULL;
}

PCB* getChildOfPpidInCVars(int ppid)
{
    CVarQueueNode* cvarNode = gCVarQueue.m_head;
    PCB* pcb = NULL;
    while(cvarNode != NULL)
    {
        pcb = getChildOfPpid(cvarNode->m_waitingQueue, ppid);
        if(pcb != NULL)
        {
            return pcb;
        }
        cvarNode = cvarNode->m_next;
    }
    return NULL;
}

PCB* getHeadProcess(PCBQueue* Q)
{
    return Q->m_head;
}


bool isEmptyProcessQueue(PCBQueue* Q)
{
    return Q->m_size == 0;
}

int getProcessQueueSize(PCBQueue* Q)
{
    return Q->m_size;
}

ExitData* exitDataDequeue(EDQueue* Q)
{
    if (Q->m_head == NULL) {
        // Empty queue
        return NULL;
    }
    else if (Q->m_head == Q->m_tail) {
        // Single item in queue
        ExitData* exitData = Q->m_head;
        Q->m_head = NULL;
        Q->m_tail = NULL;
        Q->m_size--;
        return exitData;
    } else {
        // remove from beginning
        ExitData* exitData = Q->m_head;
        Q->m_head = exitData->m_next;
        exitData->m_next = NULL;
        Q->m_size--;
        return exitData;
    }
}

void freePCB(PCB* pcb)
{
    freeRegionOneFrames(pcb);   // TODO fix these
    freeKernelStackFrames(pcb);
    exitDataFree(pcb->m_edQ);     // free exit data queue
    SAFE_FREE(pcb->m_uctx);
    SAFE_FREE(pcb->m_kctx);
    SAFE_FREE(pcb->m_pagetable);
    SAFE_FREE(pcb);
}

void freeExitedProcesses()
{
    PCB* toBeDeleted;
    while(gExitedQ.m_size > 0)
    {
        toBeDeleted = processDequeue(&gExitedQ);
        freePCB(toBeDeleted);
    }
}

void exitDataEnqueue(EDQueue* Q, ExitData* exitData)
{
    if (Q->m_head == NULL) {
        // empty list
        Q->m_head = exitData;
        Q->m_tail = exitData;
         // maybe make the processes next and prev be null?
    }
    else {
        // add to end
        Q->m_tail->m_next = exitData;
        exitData->m_prev = Q->m_tail;
        exitData->m_next = NULL;
        Q->m_tail = exitData;
    }
    Q->m_size++;
}

void exitDataFree(EDQueue* Q)
{
    ExitData* curr = Q->m_head;
    ExitData* next;
    while(curr != NULL)
    {
        next = curr->m_next;
        free(curr);
        curr = next;
    }
}
