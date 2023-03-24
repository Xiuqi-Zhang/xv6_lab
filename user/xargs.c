#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/stat.h"

int 
main(int argc, char *argv[])
{
    char *std_argv[MAXARG]; //指针数组用于保存每个命令的最终参数
    char buf[512]; //按行读取输入的缓存
    int i, tmp;

    if(argc < 2)
    {
        fprintf(2, "Usage: xargs <command>\n");
        exit(1);
    }
    if(argc + 1 > MAXARG){
        fprintf(2, "Error: too many args\n");
        exit(1);
    }

    for(int i = 1; i < argc; i++)
        std_argv[i - 1] = argv[i]; //argv[0]是xargs（不需要存），argv[1]是命令，argv[2]~argv[argc - 1]是参数
    std_argv[argc] = 0; 
    //此时std_argv[0]~std_argv[argc-2]存储着原始命令和参数，std_argv[argc-1]即将存储std参数
    //字符数组和指针数组常以0作为结尾，以防出错

    while(1)
    {
        i = 0; 
        while(1)
        {
            tmp = read(0, &buf[i], 1); //每次从标准输入中读取一个字符
            if(tmp <= 0 || buf[i] == '\n') break; //读取失败或者读到换行符则结束
            i++;
        }
        if(i == 0) break; //说明全部读完了
        buf[i] = 0; //末尾置零
        std_argv[argc - 1] = buf; //将buf添加到std_argv中

        if(fork() == 0)
        {
            exec(std_argv[0], std_argv); //子进程执行命令
            exit(0);
        }
        else wait(0);
    }
    exit(0);
}
