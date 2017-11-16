//
// Header file definitions for all functionality pertaining to the terminals
//

#ifndef __TERMINAL_H__
#define __TERMINAL_H__

#include <hardware.h>
#include <process.h>
#include <yalnix.h>

enum TermReqCode
{
    TERM_REQ_READ,
    TERM_REQ_WRITE,
    TERM_REQ_NONE
};

typedef enum TermReqCode TermReqCode;

// The terminal request is a structure that records the
// terminal read/write requests as a linked list
struct TerminalRequest
{
    TermReqCode m_code;
    PCB* m_pcb;                         // the pcb of the process that initiated this request
    void* m_bufferR0;                   // the location in R0 where data is stored as its received or sent
    void* m_bufferR1;                   // the region1 buffer location - valid only for read requests - we need to store the location where we have to write back data once we receive data from the terminal
    int m_len;                          // the size of the read or write request
    int m_serviced;                     // the amount of data that has been sent or read so far.
    int m_remaining;                    // redundant but convenient check to keep track of how much data is to processed = m_len - m_serviced
    int m_requestInitiated;             // we will use this field to tell the kernel if there has been a request that has already been started. this field is valid only within the head
    struct TerminalRequest* m_next;     // store the next request as a linked list
};

typedef struct TerminalRequest TerminalRequest;

// We have NUM_TERMINALS queues of requests
// Head nodes to the queues for the write and read requests
extern TerminalRequest gTermWReqHeads[NUM_TERMINALS];
extern TerminalRequest gTermRReqHeads[NUM_TERMINALS];

// removes a  request from the queues
// returns 0 on success, -1 on error
int removeTerminalRequest(TerminalRequest* req);

#endif
