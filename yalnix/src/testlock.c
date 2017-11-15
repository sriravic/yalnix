#include <yalnix.h>

int main(int argc, char** argv)
{
    int lock_id = -1;
    int lock_rc = LockInit(&lock_id);
    int pid = GetPid();
    if(lock_rc == ERROR)
    {
        return ERROR;
    }
    TracePrintf(0, "Process %d before fork created lock %d with rc %d.\n", pid, lock_id, lock_rc);

    int rc = Fork();
    if(rc == 0)
    {
        // child
        int cpid = GetPid();
        TracePrintf(0, "Child process %d trying to acquire lock %d.\n", cpid, lock_id);

        int acquire_rc = Acquire(lock_id);
        if(acquire_rc == ERROR)
        {
            TracePrintf(0, "Child process %d failed to acquire lock %d.\n", cpid, lock_id);
        }
        TracePrintf(0, "Child process %d successfully acquired lock %d.\n", cpid, lock_id);

        while(1)
        {
            TracePrintf(0, "Child Process : %d\n", cpid);
            Pause();
        }
    }
    else
    {
        // parent
        int ppid = GetPid();
        int acquire_rc = Acquire(lock_id);
        if(acquire_rc == ERROR)
        {
            TracePrintf(0, "Parent process %d failed to acquire lock %d.\n", ppid, lock_id);
        }
        TracePrintf(0, "Parent process %d just acquired lock %d and is holding it for a few seconds then releasing it.\n", ppid, lock_id);
        int hold_time = 5;
        while(hold_time > 0)
        {
            TracePrintf(0, "Parent Process : %d\n", ppid);
            hold_time--;
            Pause();
        }
        Release(lock_id);
        TracePrintf(0, "Parent process %d just released lock %d.\n", ppid, lock_id);
        while(1)
        {
            TracePrintf(0, "Parent Process : %d\n", ppid);
            Pause();
        }
    }
    return SUCCESS;
}
