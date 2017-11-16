#include <terminal.h>
#include <yalnixutils.h>

int removeTerminalRequest(int tty_id, TerminalRequest* request)
{
    TerminalRequest* head = NULL;
    if(request->m_code == TERM_REQ_WRITE)
        head = &gTermWReqHeads[tty_id];
    else if(request->m_code == TERM_REQ_READ)
        head = &gTermRReqHeads[tty_id];
    else { TracePrintf(0, "ERROR: Invalid terminal id supplied to remove request\n"); return -1; }

    /*
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
    */

    if(head->m_next != NULL)
    {
        SAFE_FREE(head->m_next->m_bufferR0);
        SAFE_FREE(head->m_next->m_bufferR1);
        SAFE_FREE(head->m_next);
        head->m_next = NULL;
        return 0;
    }
    else
    {
        TracePrintf(0, "ERROR: Could not find the request\n");
        return -1;
    }

}
