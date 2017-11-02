#include <terminal.h>

void addTerminalRequest(PCB* pcb, int tty_id, TermReqCode code, void* data, int len)
{
    if(tty_id < NUM_TERMINALS)
    {
        TerminalRequest* head = &gTermReqHeads[tty_id];

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
            req->m_buffer = (void*)malloc(sizeof(char) * len);
            if(req->m_buffer != NULL)
            {
                memcpy(req->m_buffer, data, sizeof(char) * len);
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

void processOutstandingWriteRequests(int tty_id)
{
    if(tty_id < NUM_TERMINALS)
    {
        TerminalRequest* head = &gTermReqHeads[tty_id];
        TerminalRequest* toProcess = head->m_next;
        if(toProcess != NULL)
        {
            if(toProcess->m_remaining == 0)
            {
                // this was a previous request that has been completed
                // free up the resources
                processEnqueue(&gWriteFinishedQ, toProcess->m_pcb);
                TerminalRequest* temp = toProcess->m_next;
                free(toProcess->m_buffer);
                free(toProcess);
                toProcess = temp;
            }

            if(toProcess != NULL)
            {
                int toSend = toProcess->m_remaining;
                toSend = toSend > TERMINAL_MAX_LINE ? TERMINAL_MAX_LINE : toSend;
                TtyTransmit(tty_id, toProcess->m_buffer + (toProcess->m_serviced), toSend);

                // update the stats
                toProcess->m_serviced += toSend;
                toProcess->m_remaining = (toProcess->m_len) - (toProcess->m_serviced);
            }
        }
    }
    else
    {
        TracePrintf(0, "Invalid terminal id supplied for processing outstanding requests\n");
    }

}
