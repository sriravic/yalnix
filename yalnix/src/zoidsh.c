// zoidsh.c - a simple shell program for the yalnix kernel
// named after our savior Zoidberg.!!!
// WOOOOOPP WOOOP WOOOOP WOOOP!!
#include <yalnix.h>

// Each shell program is invoked with the terminal that it is responsible for handling.
int main(int argc, char** argv)
{
    if(argc != 2)
    {
        TracePrintf(0, "We need a terminal number for the shell to handle.!\n");
        return -1;
    }
    else
    {
        int terminal = atoi(argv[1]);

        // print the default terminal prompt.!
        // zoidsh:
        char prompt[] = "zoidsh:";
        char buffer[TERMINAL_MAX_LINE];
        while(1)
        {
            // write out prompt
            int rc = TtyPrint(terminal, prompt, strlen(prompt));

            // wait to receive any command from the terminal
            // fork and exec the command
            // wait till child completes
            // print the output from child and then start prompting for input again.
        }

    }
}
