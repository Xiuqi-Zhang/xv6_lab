#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_trace(void)
{
    int mask;
    //syscall.c提供argint、argptr和argstr等工具函数，
    //用于在进程用户空间获得第 n 个系统调用参数。argint用于获取第n个整数，
    //存储在int指针中（通过fetchint工具函数完成）。argptr用于获取第n个整数，
    //将char指针指向它。argstr用于获取第n个整数，该参数是字符串起始地址值，
    //使用char指针指向它，函数本身返回字符串长度
    argint(0, &mask);
    struct proc *cur_proc = myproc();
    cur_proc -> trace_mask = mask;
    return 0;
}

uint64
sys_sysinfo(void)
{
    struct sysinfo info;
    struct proc *cur_proc = myproc();
    uint64 user_addr;

    info.freemem = get_freemem();
    info.nproc = cnt_procnum();

    argaddr(0, &user_addr);
    if(copyout(cur_proc->pagetable, user_addr, (char *)&info, sizeof(info)) == -1)
        return -1;
    return 0;
}
