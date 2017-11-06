#include <yalnix.h>

int main(int argc, char** argv)
{
    TracePrintf(0, "Running testmath\n");
    int a = 0/0;
    return 0;
}
