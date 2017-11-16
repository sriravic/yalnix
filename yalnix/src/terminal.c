#include <terminal.h>
#include <yalnixutils.h>

int removeTerminalRequest(TerminalRequest* request)
{
    if(request != NULL)
    {
        SAFE_FREE(request->m_bufferR0);
        SAFE_FREE(request->m_bufferR1);
        SAFE_FREE(request);
        return 0;
    }
    else
    {
        TracePrintf(MODERATE, "ERROR: NULL request provided to delete\n");
        return ERROR;
    }

}
