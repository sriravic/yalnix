#include <yalnix.h>

int main(int argc, char** argv)
{
    while(1)
    {
        TtyPrintf(0, "Hello World\n");
        Pause();
    }
    return 0;
}
