#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    if (argc > 1) {
        fprintf(2, "Usage: pingpong\n");
        exit(1);
    }
    int cc = 1;
    char buf[2];
    int p1[2], p2[2];
    
    // create a pair of pipes for both direction
    pipe(p1);
    pipe(p2);
    if(fork() == 0) {
        // child process
        close(p1[1]);
        close(p2[1]);
        read(p1[0], buf,cc);
        fprintf(1, "%d: received ping\n", getpid());
        write(p2[0], "c", 1);
        close(p1[0]);
        close(p2[0]);
    } else {
        close(p1[0]);
        close(p2[0]);
        write(p1[1], "c", 1);
        read(p2[1], buf, 1);
        wait(0);
        fprintf(1, "%d: received pong\n", getpid());
        
        close(p1[1]);
        close(p2[1]);
    }
    exit(0);
}