#include <hardware.h>
#include <yalnix.h>

int main()
{
    while(1)
	{
		unsigned int pid = GetPid();
		TracePrintf(1, "DoIdle Process - PID : %d\n");
		Pause();
	}
	return 0;
}
