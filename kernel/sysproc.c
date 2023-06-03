#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

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


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  uint64 st_addr;  //需要检测的起始地址
  uint64 mask_addr;  //用户区的位示图保存地址
  uint npages;  //需要检测的页数
  uint mask;  //位示图

  mask = 0;  //位示图清零
  pagetable_t pgtbl = myproc()->pagetable;  //需要检测的用户页表

  //从用户空间获取参数
  argaddr(0, &st_addr);
  argint(1, (int *)&npages);
  argaddr(2, &mask_addr);

  //最多检测32页
  if(npages > 64)
      return -1;

  //从起始地址所在的页表项开始遍历
  pte_t* pte = walk(pgtbl, st_addr, 0);
  for(int i = 0; i < npages; i++){
      if((pte[i] & PTE_A) && (pte[i] & PTE_V)){
          mask |= (1 << i);  //第i页的结果填入位示图的第i位
          pte[i] ^= PTE_A;  //重置访问位
      }
  }

  if(copyout(pgtbl, mask_addr, (char *)&mask, sizeof(mask)) == -1)
     return -1; //将位示图拷贝到用户区

  return 0;
}
#endif

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

