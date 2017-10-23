//
// The init process in the system
//

#include <yalnix.h>

int main(int argc, char** argv)
{
    int pid = GetPid();
    while(1)
    {
        TracePrintf(2, "init process : %d\n", pid);
        Delay(10);
    }
}
