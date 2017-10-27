/* Team Zoidberg
    A file to implement a queue of PCB data structures as a linked list
*/

#include <stdbool.h>
#include "process.h"

PCB* processDequeue(PCBQueue Q)
{
  if(Q.m_size == 0) return NULL;
  return NULL;
}

void processEnqueue(PCBQueue Q, PCB* process)
{

}

PCB* getHeadProcess(PCBQueue Q)
{
  return Q.m_head;
}


bool isEmptyProcessQueue(PCBQueue Q)
{
  return Q.m_size == 0;
}

int getSizeProcessQueue(PCBQueue Q)
{
  return Q.m_size;
}
