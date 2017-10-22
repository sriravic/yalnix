//
// The init process in the system
//

#include <yalnix.h>

int main(int argc, char** argv)
{
    while(1)
    {
        int pid = GetPid();
        TracePrintf(2, "init process : %d\n", pid);
    }
}