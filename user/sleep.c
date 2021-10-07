#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    if(argc != 2){
        fprintf(2, "Usage: sleep <number of ticks>\n");
        exit(1);
    }
    int ticks = atoi(argv[1]);
    if (ticks > 0) {
        sleep(ticks);
    }
    exit(0);
}