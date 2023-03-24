#include "kernel/types.h"
#include "user/user.h"

//xv6环境下必须要先声明函数
void sieve(int p[2]);

int 
main(int argc, char *argv[])
{
    if(argc != 1)
    {
        fprintf(2, "Usage: primes\n");
        exit(1);
    }

    int num;
    int p[2];

    //判断管道创建成功与否
    if (pipe(p) == -1) {
        fprintf(2, "Failed to create pipe.\n");
        exit(1);
    }

    if(fork() == 0)
    {
        //son process
        sieve(p);
    }
    else
    {
        //dad process
        //关闭不必要的端口
        close(p[0]);
        for(num = 2; num <= 35; num++)
        {
            write(p[1], &num, 4);
        }
        //写完后关闭写端
        close(p[1]);
        //等待子进程结束
        wait(0);
    }
    exit(0);
}

void
sieve(int p[2])
{
    int num, prime;
    int p1[2];

    close(p[1]);
    if(read(p[0], &prime, 4) == 0){
        close(p[0]);
        exit(1);
    }
    //读到的第一个数一定是素数，由祖先进程共同检验
    printf("prime %d\n", prime);

    if (pipe(p1) == -1) {
    fprintf(2, "Failed to create pipe.\n");
    exit(1);
    }

    if(fork() == 0)
    {
        //son process
        close(p[0]);
        sieve(p1);
    }
    else{
        //dad process
        close(p1[0]);
        while(1)
        {
            //如果父进程的管道为空，跳出循环
            if(read(p[0], &num, 4) == 0) break;
            //判断该数能否被prime整除
            if(num % prime != 0)
            {
                //不能整除，允许筛入下一级
                write(p1[1], &num, 4);
            }
        }
        close(p[0]);
        close(p1[1]);
        wait(0);
    }
    exit(0);
}
