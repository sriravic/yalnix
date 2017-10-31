#include <yalnix.h>

int main(int argc, char** argv)
{
    int pid = GetPid();
    while (1) {
        TracePrintf(0, "Hello World by Proces : %d\n", pid);
        Pause();
    }
}
