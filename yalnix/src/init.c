//
// The init process in the system
//

#include <yalnix.h>

int main(int argc, char** argv)
{
    int pid = GetPid();
    //int rc = Brk(&main + 6*0x2000); // test allocating some pages in the programs address space
    //rc = Brk(&main + 4*0x2000);     // test deallocating those pages
    // int status;
    // Wait(&status);
    // Wait(&status);
    while(1)
    {
        TracePrintf(0, "init process : %d\n", pid);
        Pause();
    }
}
