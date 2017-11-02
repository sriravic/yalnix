//
// Header file definitions for all functionality pertaining to the terminals
//

#ifndef __TERMINAL_H__
#define __TERMINAL_H__

#include <hardware.h>
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
    void* m_buffer;                     // the location in R0 where data is stored as its received or sent
    int m_len;                          // the size of the read or write request
    int m_serviced;                     // the amount of data that has been sent or read so far.
    int m_remaining;                    // redundant but convenient check to keep track of how much data is to processed = m_len - m_serviced
    int m_requestInitiated;             // we will use this field to tell the kernel if there has been a request that has already been started. this field is valid only within the head
    struct TerminalRequest* m_next;     // store the next request as a linked list
};

typedef struct TerminalRequest TerminalRequest;

// We have NUM_TERMINALS queues of requests
// Head nodes to the queues
extern TerminalRequest gTermReqHeads[NUM_TERMINALS];

// convenient functions
void addTerminalRequest(PCB* pcb, int tty_id, TermReqCode code, void* data, int len);
void processOutstandingWriteRequests(int tty_id);

#endif
