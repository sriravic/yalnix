// File: process.h
//
// Team: Zoidberg
//
// Description: Contains all the code necessary for process management within the yalnix system

#ifndef __PROCESS_H__
#define __PROCESS_H__

#include <hardware.h>
#include <pagetable.h>

extern int gPID;       // the global pid counter that can be given to executing processes

enum ProcessState
{
    PROCESS_START,
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_WAIT,
    PROCESS_TERMINATED
};

typedef enum ProcessState ProcessState;

// The process control block is the central structure that allows for the kernel to manage the processes.
struct ProcessControlBlock
{
    int m_pid;                  //  process id of the current running process
    int m_ppid;                 //  process id of the parent
    ProcessState m_state;       //  state of the process
    UserContext* p_uctx;        //  pointer to the user context
    PageTable* p_pt;            //  pointer to the page table for the process
    unsigned int brk;           // the brk location of this process.
};

typedef struct ProcessControlBlock PCB;

// A linked list structure to handle the different processes within the system
struct ProcessNode
{
    pcb_t* m_next;
    pcb_t* m_prev;
};

typedef struct ProcessNode ProcessNode;

// the global list of processes that the kernel will actually manage
extern ProcessNode* gStartProcessQ;
extern ProcessNode* gRunningProcessQ;
extern ProcessNode* gWaitProcessQ;
extern ProcessNode* gTerminatedProcessQ;

// hierarchical representation of process formation in the system
struct ProcessHierarchyNode
{
    
};

#endif