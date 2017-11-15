#include <yalnix.h>
#include <stdbool.h>

int main(int argc, char** argv)
{
    int lock_id = -1;
    int cvar_id = -1;
    int lock_rc = LockInit(&lock_id);
    int cvar_rc = CvarInit(&cvar_id);
    int pid = GetPid();
    if(lock_rc == ERROR || cvar_rc == ERROR)
    {
        exit(-1);
    }
    TracePrintf(0, "Process %d before fork created lock %d and cvar %d.\n", pid, lock_id, cvar_id);
    Acquire(lock_id);

    bool condition = true;

    int rc = Fork();
    if(rc == 0)
    {
        int cpid = GetPid();
        int time = 4;
        while(time >= 0)
        {
            time--;
            TracePrintf(0, "Child Process %d just decremented counter.\n", cpid);
            Pause();
        }
        TracePrintf(0, "Child Process %d about to signal cvar %d.\n", cpid, cvar_id);
        CvarSignal(cvar_id);
        while(1)
        {
            TracePrintf(0, "Child process %d.\n", cpid);
            Pause();
        }
    }
    else
    {
        int ppid = GetPid();
        while(condition)
        {
            TracePrintf(0, "Parent process %d about to wait on cvar %d.\n", ppid, cvar_id);
            int wait_rc = CvarWait(cvar_id, lock_id);
            TracePrintf(0, "Wait rc is %d.\n", wait_rc);
            if(wait_rc == ERROR)
            {
                exit(-1);
            }
            condition = false;
        }
        TracePrintf(0, "Parent process %d just finished waiting on cvar %d.\n", ppid, cvar_id);
        while(1)
        {
            TracePrintf(0, "Parent process %d.\n", ppid);
            Pause();
        }
    }


    return 0;
}
