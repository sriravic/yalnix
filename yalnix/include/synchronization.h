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
#define SYNC_SHIFT 28					// number of bits to shift to get the bits of the synchronization primitive

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

struct Pipe
{
	int m_id;			// the unique identifier for a pipe
	void* m_buffer;		// the buffer where the pipe's contents are stored
	int m_len;			// the length of the m_buffer
	int m_validLength;	// in case of reuse of pipes, we can reuse memory allocated, but make sure only data is valid till this length
};

typedef struct Pipe Pipe;

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
	int m_holder;                       // the pid of the process that holds the lock
	PCBQueue* m_waitingQueue;	// a pointer to the waiting list of processes to have the lock
	struct LockQueueNode* m_pNext;		// a pointer to the next lock that is being used within the OS
};
typedef struct LockQueueNode LockQueueNode;

// Lock functions
void lockNodeEnqueue(LockQueueNode* lockQueueNode);
void lockWaitingEnqueue(LockQueueNode* lockNode, PCB* pcb);
PCB* lockWaitingDequeue(LockQueueNode* lockNode);
LockQueueNode* getLockNode(int lockId);
int removeLockNode(LockQueueNode* lockNode);
int createLock(int pid);
int freeLock(LockQueueNode* lockNode); // to be implemented when we write kernelReclaim


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
void cvarNodeEnqueue(CVarQueueNode* cvarQueueNode);
void cvarWaitingEnqueue(CVarQueueNode* cvarQueueNode, PCB* pcb);
PCB* cvarWaitingDequeue(CVarQueueNode* cvarQueueNode);
CVarQueueNode* getCVarNode(int cvarId);
int removeCVarNode(CVarQueueNode* cvarNode);
int createCVar(int pid);
int freeCVar(CVarQueueNode* cvarNode);

// This node is used to store processes waiting on data from pipes
struct PipeQueueNode
{
	Pipe* m_pipe;
	struct PipeQueueNode* m_next;
};

struct PipeReadWaitQueueNode
{
	PCB* m_pcb;
	int m_id;
	int m_len;
	void* m_buf;
	struct PipeReadWaitQueueNode* m_next;
};

struct PipeQueue
{
	struct PipeQueueNode* m_head;
	struct PipeQueueNode* m_tail;
};

struct PipeReadWaitQueue
{
	struct PipeReadWaitQueueNode* m_head;
	struct PipeReadWaitQueueNode* m_tail;
};

typedef struct PipeQueueNode PipeQueueNode;
typedef struct PipeReadWaitQueueNode PipeReadWaitQueueNode;
typedef struct PipeQueue PipeQueue;
typedef struct PipeReadWaitQueue PipeReadWaitQueue;

void pipeEnqueue(int id);
int pipeReadWaitEnqueue(int id, int m_len, PCB* pcb, void* buff);
Pipe* getPipeNode(int id);
void processPendingPipeReadRequests();
int freePipe(int id);

// Globally defined pipes
extern LockQueue gLockQueue;			// the global lock queue
extern CVarQueue gCVarQueue;			// the global cvar queue
extern PipeQueue gPipeQueue;					// global queue for pipes
extern PipeReadWaitQueue gPipeReadWaitQueue;	// global queue for processes waiting on pipes

#endif
