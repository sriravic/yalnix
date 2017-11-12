/**
File: synchronization.h
***********************

Team: zoidberg

Description: Contains the synchronization primitives provided by yalnix to the userland processes.
*/

#ifndef __SYNCHRONIZATION_H__
#define __SYNCHRONIZATION_H__

#define LOCK_MASK 0x10000000			// we use a value of 1 for locks
#define CVAR_MASK 0x20000000			// we use a value of 2 for cvars
#define PIPE_MASK 0x30000000			// we use a value of 3 for pipes
#define SYNC_SHIFT 30					// number of bits to shift to get the bits of the synchronization primitive

#define LOCKED 1
#define UNLOCKED 0

#include "process.h"

enum SYNC_TYPE
{
	SYNC_LOCK,
	SYNC_CVAR,
	SYNC_PIPE,
	SYNC_UNDEFINED
};

typedef enum SYNC_TYPE SyncType;

extern int gSID;            // the global unique id counter that can be given to new locks/cvars/pipes

// utility functions to handle the 3 different types of synchronization primitives

// This method should be used to get unique ids for creation of any synchronization primivites
// rather than handling the gSID variable directly. This function increments, adds mask and provides the
// compound entity.
int getUniqueSyncId(SyncType t);

// This function, when passed with a compound ID, returns the correct type of synchronization primivite
SyncType getSyncType(int compoundId);

// THis function strips the compound id from its type and returns just the id
int getSyncIdOnly(int compoundId);

// A lock is a mutex that is provided to enable basic synchronization among processes.
struct Lock
{
	int m_id;			// the unique identified for the lock
	int m_owner;		// the owner process of a lock
	int m_state;		// the state of the lock - can be locked/unlocked
};
typedef struct Lock Lock;

// A condition variable is another synchronization primitive to be used for communication purposes.
struct CVar
{
	int m_id;			// the unique identifier for a condition variable.
	int m_owner;		// the owner process id of a condition variable
    int m_lockId;		// the lock id associated with the condition variable
};
typedef struct CVar CVar;
/*
struct Pipe
{
	uint32_t m_id;			// the unique identifier for a pipe
	uint32_t m_owner;		// the owner of a pipe
};
*/
// Since the synchronization primitives are all facilities provided by the kernel to
// userland processes, we are completely free to control the global list of all locks, cvars, pipes
// that are opened and closed in a sequential but safe manner
// We will maintain lists for all currently used locks in the system for interprocess communication as well

// Each lock can be used a multitude of processes that may want access to a lock.
// We control this by having a linked list of all processes waiting on a particular lock
struct LockQueue
{
	struct LockQueueNode* m_head;
    struct LockQueueNode* m_tail;
};
typedef struct LockQueue LockQueue;

struct LockQueueNode
{
	struct Lock* m_pLock;				// a pointer to the lock that is under consideration
	PCBQueue* m_waitingQueue;	// a pointer to the waiting list of processes to have the lock
	struct LockQueueNode* m_pNext;		// a pointer to the next lock that is being used within the OS
};
typedef struct LockQueueNode LockQueueNode;

// Lock functions
void lockEnqueue(LockQueueNode* lockQueueNode);
void lockWaitingEnqueue(LockQueueNode* lockNode, PCB* pcb);
PCB* lockWaitingDequeue(LockQueueNode* lockNode);
LockQueueNode* getLockNode(int lockId);
int createLock(int pid);
int deleteLock(); // to be implemented when we write kernelReclaim


// Condition variables
struct CVarQueue
{
	struct CVarQueueNode* m_head;			// the next entry in the linked list
    struct CVarQueueNode* m_tail;
};
typedef struct CVarQueue CVarQueue;

struct CVarQueueNode
{
	struct CVar* m_pCVar;				// pointer to the condition variable under consideration
	PCBQueue* m_waitingQueue;	        // the list of processes waiting on this condition variable to be satisfied
	struct CVarQueueNode* m_pNext;		// a pointer to the next condition variable used within the system.
};
typedef struct CVarQueueNode CVarQueueNode;

// CVar functions
void cvarEnqueue(CVarQueueNode* cvarQueueNode);
void cvarWaitingEnqueue(CVarQueueNode* cvarQueueNode, PCB* pcb);
PCB* cvarWaitingDequeue(CVarQueueNode* cvarQueueNode);
CVarQueueNode* getCVarQueueNode(int cvarId);
int createCVar(int pid);
int deleteCVar(); // to be implemented when we write kernelReclaim


/*
// Pipe handling code
// We have two global queues for pipes. One for all the pipes that have writing processes to them
// and other queues for processes reading from pipes. For now we assume that one process can read and one process can
// write to a pipe at time. We are not sure about multi-process pipes.
struct WritePipeQueue
{
	Pipe* m_pipe;						// a pointer to a pipe that is writing to memory
	void* p_mem;						// the pointer to where within the kerneld data will this pipe's contents be moved to
	struct WritePipeQueue* m_next;		// a pointer to the next active pipe within the system
	int m_status;						// a status flag to indicate if we are done writing to the pipe so that the OS can do appropriate stuff
};

struct ReadPipeQueue
{
	Pipe* m_pipe;						// the pointer the pipe from which data will be read
	void* p_mem;						// location in kernel memory where the pipe's data resides
	struct ReadPipeQueue* m_next;		// a pointer to the next entry in the pipe queue that is a read pipe process
	int m_status;						// the status of the particular pipe
};
*/

// Globally defined pipes
extern LockQueue gLockQueue;			// the global lock queue
extern CVarQueue gCVarQueue;			// the global cvar queue
// struct WritePipeQueue* gWritePipeQueue;		// the glboal write pipe queue
// struct ReadPipeQueue* gReadPipeQueue;		// the global read pipe queue

#endif
