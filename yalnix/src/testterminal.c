#include <yalnix.h>

void testWrite()
{
    const int size = 8196;
    char test[size];
    char string[] = "helloworld";
    int len = strlen(string);
    int i;
    for(i = 0; i < size - 2; i++)
    {
        int idx = i % len;
        test[i] = string[idx];
    }

    // null terminate the string
    test[i] = '\0';
    test[i+1] = '\n';

    while(1)
    {
        TtyPrintf(0, string);
        Pause();
    }
}

void testRead()
{
    char prompt[] = "Reading From Terminal 0 : ";
    TtyPrintf(0, prompt);
    char data[200];
    TtyRead(0, data, 200);

    char prompt1[] = "Data Read : ";
    TtyPrintf(0, prompt1);
    TtyPrintf(0, data);
}

int main(int argc, char** argv)
{
    while(1)
    {
        testRead();
        TracePrintf(0, "TestTerminal Program.!\n");
        Pause();
    }
    return 0;
}
