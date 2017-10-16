#include <yalnix.h>
#include <hardware.h>

int main()
{
    while(1)
	{
		TracePrintf(1, "DoIdle\n");
		Pause();
	}
}