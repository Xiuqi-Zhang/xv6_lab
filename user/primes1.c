#include "kernel/types.h"
#include "user/user.h"

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
    pipe(p);
    if(fork() == 0)
    {
        sieve(p);
    }
    else
    {
        close(p[0]);
        for(num = 2; num <= 35; num++)
        {
            write(p[1], &num, 4);
        }
        close(p[1]);
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
    printf("prime %d\n", prime);
    pipe(p1);
    if(fork() == 0)
    {
        close(p[0]);
        sieve(p1);
    }
    close(p1[0]);
    while(1)
    {
        if(read(p[0], &num, 4) == 0) break;
        if(num % prime != 0)
        {
            write(p1[1], &num, 4);
        }
    }
    close(p[0]);
    close(p1[1]);
    wait(0);
    exit(0);
}
