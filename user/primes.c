#include "kernel/types.h"
#include "user/user.h"

#define R 0
#define W 1

void redirect_descriptor(int flag, int p[])
{
    // 重定向
    // 让每个进程 只使用 0,1 两个文件描述符来传递素数。
    close(flag);
    dup(p[flag]);
    close(p[0]);
    close(p[1]);
}

void prime_pipe(int p[]) {
    // p 是用来和父进程通信的管道， p2 是和子进程通信的管道
    int prime = 1;
    redirect_descriptor(R, p);
    read(R, &prime, sizeof(prime)); // get a number from left neighbor
    printf("prime %d\n", prime);
    int val = 0;
    // create the processes in the pipeline only as they are needed.
    if (read(R, &val, sizeof(val)) != 0) { // get a number from left neighbor
        int p2[2]; // 
        pipe(p2);
        if (fork() == 0) {
            // grandchild process
            prime_pipe(p2);
        } else {
            redirect_descriptor(W, p2);
            if (val % prime != 0) {
                write(W, &val, sizeof(val));
            }
            while (1) {
                if (read(R, &val, sizeof(val)) != 0) { // get a number from left neighbor
                    if (val % prime == 0) {
                        // drop it
                        continue;
                    } else {
                        // writes to its right neighbor over another pipe
                        write(W, &val, sizeof(val));
                    }
                } else {
                    break;
                }
            }
            close(W);
        }
    }
    close(R);
    wait(0);
    exit(0);
}

int
main(int argc, char *argv[]) {

    // 每个子进程都和自己的child process 使用1个管道。
    int p[2];
    pipe(p);
    // p[0] ---> p[1];
    if (fork() == 0) {
        // 子进程
        prime_pipe(p);
    } else {
        redirect_descriptor(W, p);
        for (int i=2; i<=35; i++) {
            write(W, &i, sizeof(i));
        }
        close(W);
        wait(0);
    }
    exit(0);
}