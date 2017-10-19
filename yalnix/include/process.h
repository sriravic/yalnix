// File: process.h
//
// Team: Zoidberg
//
// Description: Contains all the code necessary for process management within the yalnix system

#ifndef __PROCESS_H__
#define __PROCESS_H__

#include <hardware.h>

struct Pcb
{
    unsigned int m_pid;
    unsigned int m_ppid;
};

typedef struct Pcb pcb_t;

// A linked list structure to handle the different processes within the system
struct ProcessNode
{
    pcb_t* m_next;
    pcb_t* m_prev;
};

typedef struct ProcessNode pnode_t;

// the global list of processes that the kernel will actually manage
extern pnode_t* gReadyToRunProcesses;
extern pnode_t* gInputBlockedProcesses;
extern pnode_t* gOuputBlockedProcesses;
extern pnode_t* gSysCallBlockedProcesses;
extern pnode_t* gRunningProcesses;

// hierarchical representation of process formation in the system
struct ProcessHierarchyNode
{

};

#endif