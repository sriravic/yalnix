#include <terminal.h>

void addTerminalWriteRequest(PCB* pcb, int tty_id, TermReqCode code, void* data, int len)
{
    if(tty_id < NUM_TERMINALS)
    {
        TerminalRequest* head = &gTermWReqHeads[tty_id];

        // find the first empty slot
        TerminalRequest* curr = head;
        TerminalRequest* next = curr->m_next;
        while(next != NULL)
        {
            curr = next;
            next = curr->m_next;
        }

        // create the new entry for this request
        TerminalRequest* req = (TerminalRequest*)malloc(sizeof(TerminalRequest));
        if(req != NULL)
        {
            curr->m_next = req;
            req->m_code = code;
            req->m_pcb = pcb;
            req->m_bufferR0 = (void*)malloc(sizeof(char) * len);
            if(req->m_bufferR0 != NULL)
            {
                memcpy(req->m_bufferR0, data, sizeof(char) * len);
                req->m_len = len;
                req->m_serviced = 0;
                req->m_remaining = len;
                req->m_next = NULL;
            }
            else
            {
                TracePrintf(0, "Error could not allocate memory for storing the terminal request\n");
            }
        }
        else
        {
            TracePrintf(0, "Error: Couldnt allocate memory for terminal request");
        }
    }
    else
    {
        TracePrintf(0, "Invalid terminal number provided\n");
    }
}

void addTerminalReadRequest(PCB* pcb, int tty_id, TermReqCode code, void* data, int len)
{
    if(tty_id < NUM_TERMINALS)
    {
        TerminalRequest* head = &gTermRReqHeads[tty_id];

        // find the first empty slot
        TerminalRequest* curr = head;
        TerminalRequest* next = curr->m_next;
        while(next != NULL)
        {
            curr = next;
            next = curr->m_next;
        }
        // create the new entry for this request
        TerminalRequest* req = (TerminalRequest*)malloc(sizeof(TerminalRequest));
        if(req != NULL)
        {
            curr->m_next = req;
            req->m_code = code;
            req->m_pcb = pcb;
            req->m_bufferR1 = data;             // store the location where the data once its received should be stored back
            if(req->m_bufferR1 != NULL)
            {
                req->m_len = len;
                req->m_serviced = 0;
                req->m_remaining = 0;
                req->m_next = NULL;
            }
            else
            {
                TracePrintf(0, "Invalid location provided for read request in R1 region for process : %d\n", pcb->m_pid);
            }
        }
    }
    else
    {
        TracePrintf(0, "Invalid terminal number provided\n");
    }
}

void processOutstandingWriteRequests(int tty_id)
{
    if(tty_id < NUM_TERMINALS)
    {
        TerminalRequest* head = &gTermWReqHeads[tty_id];
        TerminalRequest* toProcess = head->m_next;
        if(toProcess != NULL)
        {
            if(toProcess->m_remaining == 0)
            {
                // this was a previous request that has been completed
                // free up the resources
                processEnqueue(&gWriteFinishedQ, toProcess->m_pcb);
                processRemove(&gWriteBlockedQ, toProcess->m_pcb);
                TerminalRequest* temp = toProcess->m_next;
                free(toProcess->m_bufferR0);
                free(toProcess);
                toProcess = temp;
            }

            if(toProcess != NULL)
            {
                int toSend = toProcess->m_remaining;
                toSend = toSend > TERMINAL_MAX_LINE ? TERMINAL_MAX_LINE : toSend;
                TtyTransmit(tty_id, toProcess->m_bufferR0 + (toProcess->m_serviced), toSend);

                // update the stats
                toProcess->m_serviced += toSend;
                toProcess->m_remaining = (toProcess->m_len) - (toProcess->m_serviced);
            }
            else
            {
                // no more processes to initate the transmit to terminal
                // set the flag
                head->m_requestInitiated = 0;
            }
        }
    }
    else
    {
        TracePrintf(0, "Invalid terminal id supplied for processing outstanding requests\n");
    }

}

void processOutstandingReadRequests(int tty_id)
{
    if(tty_id < NUM_TERMINALS)
    {
        TerminalRequest* head = &gTermWReqHeads[tty_id];
        TerminalRequest* toProcess = head->m_next;
        if(toProcess != NULL)
        {
            // allocate maximum memory of TERMINAL_LINE_LENGTH
            toProcess->m_bufferR0 = (void*)malloc(sizeof(char) * TERMINAL_MAX_LINE);
            if(toProcess->m_bufferR0 != NULL)
            {
                int dataReceived = TtyReceive(tty_id, toProcess->m_bufferR0, TERMINAL_MAX_LINE);
                toProcess->m_remaining = dataReceived;
                toProcess->m_serviced = 0;

                // we have received the data
                // we will send the data to the process that requested data
                // when we schedule it to run
                processRemove(&gReadBlockedQ, toProcess->m_pcb);
                processEnqueue(&gReadFinishedQ, toProcess->m_pcb);
                toProcess->m_pcb->m_iodata = toProcess;
                head->m_next = toProcess->m_next;
            }
            else
            {
                TracePrintf(0, "Error allocating memory for reading to R0 memory from terminal\n");
            }
        }
        else
        {
            // No process is waiting for input
            // ignore the data that was received.!!
            TracePrintf(0, "No user process is waiting for input from the terminal. Ignoring\n");
        }
    }
}

int removeTerminalRequest(int tty_id, TerminalRequest* request)
{
    TerminalRequest* head = NULL;
    if(request->m_code == TERM_REQ_WRITE)
        head = &gTermWReqHeads[tty_id];
    else if(request->m_code == TERM_REQ_READ)
        head = &gTermRReqHeads[tty_id];
    else { TracePrintf(0, "ERROR: Invalid terminal id supplied to remove request\n"); return -1; }

    TerminalRequest* prev = head->m_next;
    TerminalRequest* curr = prev;
    while(curr != request && curr != NULL)
    {
        prev = curr;
        curr = curr->m_next;
    }

    if(curr == request && curr != NULL)
    {
        TracePrintf(2, "INFO: Found the request. Freeing it\n");
        prev->m_next = curr->m_next;
        if(curr->m_bufferR0 != NULL) free(curr->m_bufferR0);
        if(curr->m_bufferR1 != NULL) free(curr->m_bufferR1);
        free(curr);
    }
    else
    {
        TracePrintf(2, "INFO: Could not locate the requst\n");
        return -1;
    }
}
