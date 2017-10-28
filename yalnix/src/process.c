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
        return pcb;
    } else {
        // remove from beginning
        PCB* pcb = Q->m_head;
        Q->m_head = pcb->m_next;
        pcb->m_next = NULL;
        Q->m_size--;
        return pcb;
    }
}

void processEnqueue(PCBQueue* Q, PCB* process)
{
    if (Q->m_head == NULL) {
        // empty list
        Q->m_head = process;
        Q->m_tail = process;
         // maybe make the processes next and prev be null?
    }
    else {
        // add to end
        Q->m_tail->m_next = process;
        process->m_prev = Q->m_tail;
        process->m_next = NULL;
        Q->m_tail = process;
    }
    Q->m_size++;
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
