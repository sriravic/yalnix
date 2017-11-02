#include <terminal.h>

void AddTerminalRequest(PCB* pcb, int tty_id, TermReqCode code, void* data, int len)
{
    if(tty_id < NUM_TERMINALS)
    {
        TerminalRequest head = gTermReqHeads[tty_id];

        // find the first empty slot
        TerminalRequest* curr = head.m_next;
        TerminalRequest* next = curr;
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
        TracePrintf(0, "Error: Couldnt allocate memory for terminal request");
    }
    else
    {
        TracePrintf(0, "Invalid terminal number provided\n");
    }
}
