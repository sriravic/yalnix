#include <yalnix.h>

int main(int argc, char** argv)
{
    int rc = Fork();
    if(rc == 0)
    {
        TracePrintf(0, "Fork success - child process - Exec-ing now\n");
        char* args[] = {"helloworld"};
        rc = Exec("helloworld", args);
        TracePrintf(0, "Exec failed\n");
        return -1;
    }
    else
    {
        TracePrintf(0, "Fork Success - Parent Process\n");
        int pid = GetPid();
        while(1)
        {
            TracePrintf(0, "Process : %d\n", pid);
            Pause();
        }
    }
    return 0;
}
