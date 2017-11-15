#include <yalnix.h>

int main()
{
	int pid = GetPid();
    while(1)
	{
		TracePrintf(1, "DoIdle Process - PID : %d\n", pid);
		Delay(2);
	}
	return 0;
}
