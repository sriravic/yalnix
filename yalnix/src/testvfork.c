#include <yalnix.h>

int main(int argc, char** argv)
{
    int rc = VFork();
    TracePrintf(0, "RETURN CODE IS %d.\n", rc);
    if(rc == 0)
    {
        TracePrintf(0, "fork succeeded : Child process\n");
        int cpid = GetPid();
        while(1)
        {
            TracePrintf(0, "Child Process : %d\n", cpid);
            Pause();
        }
    }
    else
    {
        TracePrintf(0, "fork succeeded : parent process\n");
        int ppid = GetPid();
        while(1)
        {
            TracePrintf(0, "Parent Process : %d\n", ppid);
            Pause();
        }
    }
    return 0;
}
