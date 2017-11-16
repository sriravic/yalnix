#include "yalnix.h"

int main(int argc, char** argv)
{
    int pid = GetPid();
    int status;

    TracePrintf(0, "Parent process %d will wait with no children and no exited children.\n", pid);
    int rc = Wait(&status);
    TracePrintf(0, "Parent process %d done waiting: expected rc=-1, actual rc=%d. Expected status=-1, actual status=%d.\n", pid, rc, status);

    int fork_rc = Fork();
    if(fork_rc == 0)
    {
        Delay(5);
        TracePrintf(0, "Child process %d will now exit to give its parent something to do.\n", pid);
        Exit(17);
    }
    else
    {
        TracePrintf(0, "Parent process %d will wait with one child but no exited children.\n", pid);
        rc = Wait(&status);
        TracePrintf(0, "Parent process %d done waiting: expected rc=(child's pid), actual rc=%d. Expected status=17, actual status=%d.\n", pid, rc, status);
    }

    TracePrintf(0, "Parent process %d will now spawn 5 children that will exit.\n", pid);
    int fork_num = 5;
    int i;
    for(i = 1; i <= fork_num; i++)
    {
        fork_rc = Fork();
        if(fork_rc == 0)
        {
            TracePrintf(0, "Child process fork %d will now exit.\n", i);
            Exit(i);
        }
    }

    TracePrintf(0, "Parent process %d will wait on its 5 children.\n", pid);
    for(i = 1; i <= fork_num; i++)
    {
        rc = Wait(&status);
        TracePrintf(0, "Parent process %d done waiting: rc=%d, status=%d.\n", pid, rc, status);
    }

    return 0;
}
