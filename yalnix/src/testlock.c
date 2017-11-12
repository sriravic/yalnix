#include <yalnix.h>

int main(int argc, char** argv)
{
    int lock_id = -1;
    int lock_rc = LockInit(&lock_id);
    int pid = GetPid();
    if(lock_rc == ERROR)
    {
        return -1;
    }
    TracePrintf(0, "Process %d before fork created lock %d with rc %d.\n", pid, lock_id, lock_rc);

    int rc = Fork();
    TracePrintf(0, "RETURN CODE IS %d.\n", rc);
    if(rc == 0)
    {
        // child
        int cpid = GetPid();
        TracePrintf(0, "Child process %d trying to acquire lock %d.\n", cpid, lock_id);
        // int acquire_rc = Acquire(lock_id);
        // if(acquire_rc == SUCCESS)
        // {
        //     TracePrintf(0, "Child process %d successfully acquired lock %d.\n", cid, lock_id);
        // }
    }
    else
    {
        // parent
        int ppid = GetPid();
        TracePrintf(0, "Parent process %d holding lock for a few seconds then releasing it.\n", ppid);
        int hold_time = 3;
        while(hold_time > 0)
        {
            TracePrintf(0, "Parent Process : %d\n", ppid);
            hold_time--;
            Pause();
        }
        Release(lock_id);
        while(1)
        {
            TracePrintf(0, "Parent Process : %d\n", ppid);
            Pause();
        }
    }
    return 0;
}
