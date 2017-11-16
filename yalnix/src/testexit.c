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
        TracePrintf(0, "Parent process %d done waiting: expected rc=0, actual rc=%d. Expected status=17, actual status=%d.\n", pid, rc, status);
    }

    return 0;
}
