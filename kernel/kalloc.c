// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

//假定偷取页面的上限是64页
#define STEAL_CNT 64

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

// kernel/kalloc.c
struct {
  struct spinlock lock;
  struct run *freelist;
  uint64 st[STEAL_CNT];
} kmem[NCPU]; // 为每个 CPU 分配独立的 freelist，并用独立的锁保护它。

char *kmem_lock_names[] = {
  "kmem_cpu_0",
  "kmem_cpu_1",
  "kmem_cpu_2",
  "kmem_cpu_3",
  "kmem_cpu_4",
  "kmem_cpu_5",
  "kmem_cpu_6",
  "kmem_cpu_7",
};

void
kinit()
{
  for(int i=0;i<NCPU;i++) { // 初始化所有锁
    initlock(&kmem[i].lock, kmem_lock_names[i]);
  }
  freerange(end, (void*)PHYSTOP);
}


void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
// kernel/kalloc.c
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  int cpu = cpuid();
  pop_off();

  acquire(&kmem[cpu].lock);
  r->next = kmem[cpu].freelist;
  kmem[cpu].freelist = r;
  release(&kmem[cpu].lock);
}

//这个函数用于为指定CPU偷取空闲页面
int 
steal(uint cpu)
{
    int remain = STEAL_CNT;  //remain表示还要偷取多少页面
    int idx = 0;  //idx表示已偷取页面

    for(int i = 0; i < NCPU; i++){
        if(i == cpu)  //跳过自己，避免自己偷自己情况
            continue;
        acquire(&kmem[i].lock);  //获取目标CPU的锁
        while(kmem[i].freelist && remain){ //若目标CPU有空闲页面且我们还需要偷
            kmem[cpu].st[idx++] = (uint64)kmem[i].freelist; //将空闲页面保存到暂存数组
            kmem[i].freelist = kmem[i].freelist->next;  //目标CPU的空闲页面减1
            remain--;  //还需要偷的页面数减1
        }
        release(&kmem[i].lock); //释放目标CPU的锁
        if(remain == 0)  //若已偷完，结束；否则继续偷下一个CPU
            break;
    }
    return idx;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off(); //为了防止steal()函数的重复调用，需要关中断

  int cpu = cpuid();
  
  acquire(&kmem[cpu].lock); //获取本CPU的锁
  r = kmem[cpu].freelist; 
  if(r){ //若本CPU有空闲页面
      kmem[cpu].freelist = r->next;
      release(&kmem[cpu].lock);  //释放本CPU的锁
  }else{ //若无，则需要偷取
      release(&kmem[cpu].lock);  //释放本CPU的锁，防止死锁
      int stealed = steal(cpu);  //偷取空闲页面
      if(stealed <= 0){  //若无空闲页面，返回0，kalloc()失败
          pop_off();
          return 0;
      }
      acquire(&kmem[cpu].lock);  //获取本CPU的锁
      for(int i = 0; i < stealed; i++){  //依次将暂存队列中的空闲页面加入freelist中
          ((struct run *)kmem[cpu].st[i])->next = kmem[cpu].freelist;
          kmem[cpu].freelist = (struct run *)kmem[cpu].st[i];
      }
      r = kmem[cpu].freelist; //获取空闲页面
      kmem[cpu].freelist = r->next;
      release(&kmem[cpu].lock); //释放本CPU的锁
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  
  pop_off(); //开中断
  return (void*)r;
}                                                                                          
