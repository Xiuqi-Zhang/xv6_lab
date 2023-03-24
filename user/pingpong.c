#include "kernel/types.h"
#include "user/user.h"

int 
main(int argc, char *argv[])
{
    int p1[2], p2[2];
    char msg = 'a';
    int size = sizeof(msg);
    pipe(p1);
    pipe(p2);

    if(fork() > 0)
    {
        //dad process
        //关闭p1的读端，p2的写端，关闭不必要的端口，防止读写时堵塞
        close(p1[0]);
        close(p2[1]);

        if(write(p1[1], &msg, size) != size)
        {
            printf("dad send msg to son went wrong!\n");
            exit(1);
        }

        if(read(p2[0], &msg, size) != size)
        {
            printf("dad receive msg from son went wrong!\n");
            exit(1);
        }

        printf("%d: received pong\n", getpid());

        wait(0); //0表示父进程不在乎子进程的退出状态

        exit(0);
    }
    
    //son process
    close(p1[1]);
    close(p2[0]);

    if(read(p1[0], &msg, size) != size)
    {
        printf("son receive msg from dad went wrong!\n");
        exit(1);
    }

    printf("%d: received ping\n", getpid());

    if(write(p2[1], &msg, size) != size)
    {
        printf("son send msg to dad went wrong!\n");
        exit(1);
    }

    exit(0);
}
