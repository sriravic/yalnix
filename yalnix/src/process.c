/* Team Zoidberg
    A file to implement a queue of PCB data structures as a linked list
*/

#include <stdbool.h>
#include "process.h"

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
PCB* getPcbByPid(PCBQueue* Q, unsigned int pid)
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

// return the first PCB that is a child of the given ppid, or NULL if there is not one
PCB* getChildOfPpid(PCBQueue* Q, unsigned int ppid)
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

// We will use this function to remove a PCB from one of the queues
// ultimately we would want one that simultaneuously picks one from one queue
// removes it and then adds to the other queue
void removeFromQueue(PCBQueue* Q, PCB* process)
{
    if(Q->m_head == NULL)
    {
        TracePrintf(0, "ERROR: There was no process in the queue to be removed.!");
    }
    else if(Q->m_head == Q->m_tail)
    {
        if(Q->m_head->m_pid = process->m_pid)
        {
            // we found the process
            Q->m_head = NULL;
            Q->m_tail = NULL;
        }
        else
        {
            TracePrintf(0, "WARNING: Process intended to be removed was not found in the queue.!");
        }
    }
    else
    {
        PCB* curr = Q->m_head;
        PCB* next = Q->m_head;
        while(curr != NULL)
        {
            if(next->m_pid == process->m_pid)
            {
                curr->m_next = next->m_next;
                next->m_next = NULL;
                next->m_prev = NULL;
            }
            else
            {
                curr = next;
                next = next->m_next;
            }
        }
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
