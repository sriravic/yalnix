#include <stdio.h>

main(argc, argv)
int argc;
char *argv[];
{
    int pid;

    TracePrintf(0,"BEFORE\n");

    if ((pid = Fork()) == 0)
    {
      TracePrintf(0,"CHILD\n");
      recurse("child", 5, 2);
    }
    else
    {
      TracePrintf(0,"PARENT: child pid = %d\n", pid);
      recurse("parent", 5, 1);
    }

    Exit(0);
}

recurse(who, i, tty_id)
char *who;
int i;
int tty_id;
{
    char waste[1024];	/* use up stack space in the recursion */
    char *mem = (char *)malloc(2048); /* use up heap space */
    int j;

    for (j = 0; j < 1024; j++)
     waste[j] = 'a';

    TtyPrintf(tty_id, "%s %d\n", who, i);
    if (i == 0)
    {
	TtyPrintf(tty_id, "Done with recursion\n");
	return;
    }
    else
	recurse(who, i - 1, tty_id);
}
