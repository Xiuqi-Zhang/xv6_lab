#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int 
main(int argc, char *argv[])
{
    if(argc != 2)
    {
        fprintf(2, "Usage: sleep ticks\n");
        exit(1);
    }

    int ticks = atoi(argv[1]);
    fprintf(1, "Sleep %d\n", ticks);
    sleep(ticks);
    exit(0);
}
