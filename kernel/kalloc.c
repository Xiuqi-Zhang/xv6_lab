// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct spinlock reflock;

//给从KERNBASE开始的每一个页框进行编号
//最大编号在PHYSTOP处
//将每个页框的引用计数存在对应编号的数组中
#define PA2REFID(_pa) ((((uint64)_pa) - KERNBASE) / PGSIZE)
#define MAX_REFID PA2REFID(PHYSTOP)
#define PG_REFCNT(_pa) pg_refcnt[PA2REFID(_pa)]

int pg_refcnt[MAX_REFID];

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&reflock, "reflock"); //锁用于引用计数减少的互斥
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

void
refcnt_inc(void* pa){
  acquire(&reflock);
  PG_REFCNT(pa)++;
  release(&reflock);
}
// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&reflock); //加锁的原因是防止多个进程同时kfree导致错误
  PG_REFCNT(pa)--; //引用计数减1
  if(PG_REFCNT(pa) <= 0){  //若引用计数减至0，真正释放内存
      // Fill with junk to catch dangling refs.
      memset(pa, 1, PGSIZE);

      r = (struct run*)pa;

      acquire(&kmem.lock);
      r->next = kmem.freelist;
      kmem.freelist = r;
      release(&kmem.lock);
  }
  release(&reflock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
    PG_REFCNT((uint64)r) = 1; //引用计数设为1
  }
  return (void*)r;
}
