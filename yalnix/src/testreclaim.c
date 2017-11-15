#include <yalnix.h>

int main(int argc, char** argv)
{
    int cvar_rc, lock_rc, pipe_rc;
    int n = 4;
    int cvars[n];
    int locks[n];
    int pipes[n];

    // populate the arrays
    int i, j;
    for(i = 0; i < n; i++)
    {
        cvar_rc = CvarInit(&cvars[i]);
        lock_rc = LockInit(&locks[i]);
        pipe_rc = PipeInit(&pipes[i]);
        if(cvar_rc == ERROR || lock_rc == ERROR || pipe_rc)
        {
            exit(-1);
        }
    }

    // To exercies all branches, remove the last thing then all the others before it
    for(i = n-1; i < 2*n-1; i++)
    {
        j = i%n;
        TracePrintf(0, "Iteration %d, pos %d, reclaiming cvar %d, lock %d, and pipe %d.\n", i-n+1, j, cvars[j], locks[j], pipes[j]);
        cvar_rc = Reclaim(cvars[j]);
        lock_rc = Reclaim(locks[j]);
        pipe_rc = Reclaim(pipes[j]);
        if(cvar_rc == ERROR || lock_rc == ERROR || pipe_rc == ERROR)
        {
            TracePrintf(0, "Error codes: cvar: %d, lock: %d, pipe: %d.\n", cvar_rc, lock_rc, pipe_rc);
            exit(-1);
        }
    }


    TracePrintf(0, "Reclaim successful.\n");
    return 0;
}
