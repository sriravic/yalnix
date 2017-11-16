// File: process.h
//
// Team: Zoidberg
//
// Description: Contains all the code necessary for process management within the yalnix system

#ifndef __PROCESS_H__
#define __PROCESS_H__

#include <hardware.h>
#include <pagetable.h>
#include <stdbool.h>

extern int gPID;            // the global pid counter that can be given to executing processes
extern void* gKernelBrk;    // the global kernel brk

// The process control block is the central structure that allows for the kernel to manage the processes.
struct ProcessControlBlock
{
    int m_pid;                                      //  process id of the current running process
    int m_ppid;                                     //  process id of the parent
    UserContext* m_uctx;                            //  pointer to the user context
    KernelContext* m_kctx;                          //  pointer to the kernel context;
    UserProgPageTable* m_pagetable;                 //  pointer to the actual user mode page table.
    unsigned int m_brk;                             // the brk location of this process.
    unsigned int m_ticks;                           // increment the number of ticks this process has been running for
    unsigned int m_timeToSleep;                     // how long we expect to sleep for
    struct ProcessControlBlock* m_next;             // doubly linked list next pointers
    struct ProcessControlBlock* m_prev;             // doubly linked list prev pointers
    struct ExitDataQueue* m_edQ;                    // singly linked list of exit data
    char* m_name;                                   // name of the process
};

typedef struct ProcessControlBlock PCB;

// A linked list structure to handle the different processes within the system
struct PCBQueue
{
    PCB* m_head;
    PCB* m_tail;
    int m_size;
};

typedef struct PCBQueue PCBQueue;

// the global list of processes that the kernel will actually manage
extern PCBQueue gRunningProcessQ;
extern PCBQueue gReadyToRunProcessQ;
extern PCBQueue gWaitProcessQ;
extern PCBQueue gTerminatedProcessQ;
extern PCBQueue gSleepBlockedQ;
extern PCBQueue gWriteBlockedQ;
extern PCBQueue gWriteFinishedQ;
extern PCBQueue gWriteWaitQ;                    // waiting for a chance to write to a terminal
extern PCBQueue gReadBlockedQ;
extern PCBQueue gReadFinishedQ;
extern PCBQueue gExitedQ;

// Function headers defined in process.c
PCB* processDequeue(PCBQueue* Q);
void processEnqueue(PCBQueue* Q, PCB* process);
void processRemove(PCBQueue* Q, PCB* process);
PCB* getPcbByPid(PCBQueue* Q, int pid);
PCB* getPcbByPidInLocks(int pid);
PCB* getPcbByPidInCVars(int pid);
PCB* getChildOfPpid(PCBQueue* Q, int ppid);
PCB* getChildOfPpidInLocks(int ppid);
PCB* getChildOfPpidInCVars(int ppid);
PCB* getHeadProcess(PCBQueue* Q);
bool isEmptyProcessQueue(PCBQueue* Q);
int getProcessQueueSize(PCBQueue* Q);
void removeFromQueue(PCBQueue* Q, PCB* process);
void freePCB(PCB* pcb);
void freeExitedProcesses();

// Struct for keeping track of the data of a terminated process
struct ExitData
{
    int m_pid;
    int m_status;
    struct ExitData* m_next;
    struct ExitData* m_prev;
};

typedef struct ExitData ExitData;

struct ExitDataQueue {
    ExitData* m_head;
    ExitData* m_tail;
    int m_size;
};

typedef struct ExitDataQueue EDQueue;

// Temporary global list of ExitData. Eventually, each process will have a list
extern EDQueue gExitDataQ;

// Function headers for the exit data queue
ExitData* exitDataDequeue(EDQueue* Q);
void exitDataEnqueue(EDQueue* Q, ExitData* exitData);
void exitDataFree(EDQueue* Q);

#endif
