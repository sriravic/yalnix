#include <yalnix.h>

int main()
{
    while(1)
	{
		TracePrintf(0, "Idle Process.!!");
		//unsigned int pid = GetPid();
		TracePrintf(1, "DoIdle Process - PID : %d\n");
		Pause();
	}
	return 0;
}
