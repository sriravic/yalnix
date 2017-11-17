#include <yalnix.h>

int main(int argc, char** argv)
{

    int rc = Fork();
    if(rc == 0)
    {
        rc = Fork();
        if(rc == 0)
        {
            char* args[] = {"testexec"};
            rc = Exec("testexec", args);
        }
        else
        {
            char* args[] = {"helloworld"};
            rc = Exec("helloworld", args);
        }
    }
    else
    {
        // go for some 100 iterations for the processs to be created
        int i;
        for(i = 0; i < 6; i++)
        {
            Pause();
        }

        PS(0);
    }
    return 0;
}
