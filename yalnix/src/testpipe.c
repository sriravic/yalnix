#include <yalnix.h>

int main(int argc, char** argv)
{
    int pipeId;
    int rc = PipeInit(&pipeId);
    char msg[] = "Hello World From Parent";
    if(rc == SUCCESS)
    {
        int krc = Fork();
        if(krc == 0)
        {
            int cpid = GetPid();
            TracePrintf(0, "Child Process :%d reading from pipe\n", cpid);
            char buff[strlen(msg)];
            int len = PipeRead(pipeId, buff, strlen(msg));
            if(len != strlen(msg))
            {
                TracePrintf(0, "Child was not able to read %d bytes from parent\n", strlen(msg));
            }
            else
            {
                TracePrintf(0, "Child Read : %s\n", buff);
                while(1)
                {
                    TracePrintf("Child Process : %d\n", cpid);
                    Pause();
                }
            }
        }
        else
        {
            int ppid = GetPid();
            TracePrintf(0, "Parent Process : %d writing to pipe\n", ppid);
            int len = PipeWrite(pipeId, msg, strlen(msg));
            if(len != strlen(msg))
            {
                TracePrintf(0, "Pipe Write Failed\n");
            }
            while(1)
            {
                TracePrintf(0, "Parent process : %d\n", ppid);
                Pause();
            }
        }
    }
    else
    {
        TracePrintf(0, "Error creating pipes\n");
        exit(-1);
    }

    return 0;
}
