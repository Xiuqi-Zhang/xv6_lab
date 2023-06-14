// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define BUFMAP_SIZE 13
#define BUFMAP_HASH(dev, blockno) (((dev << 27) | blockno) % BUFMAP_SIZE)

struct {
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct spinlock bufmap_lock; //确保偷取过程的互斥，避免产生为一个磁盘块分配多个缓存的情况
  struct buf bufmap[BUFMAP_SIZE];
  struct spinlock bufmap_bucket_lock[BUFMAP_SIZE];
} bcache;

//初始化缓存
void
binit(void)
{
  struct buf *b;

  //初始化哈希表以及各桶锁
  for(int i = 0; i < BUFMAP_SIZE; i++){
      initlock(&bcache.bufmap_bucket_lock[i], "bcache_bucket_locks");
      bcache.bufmap[i].next = 0;
  }

  //初始化各空缓存块
  for(int i = 0; i < NBUF; i++){
    b = &bcache.buf[i];
    b->refcnt = 0;
    b->lastuse = 0;
    initsleeplock(&b->lock, "buffer"); //初始化各缓存块的睡眠锁
    b->next = bcache.bufmap[0].next; //暂时将所有缓存块放在0号桶中
    bcache.bufmap[0].next = b;
  }

  initlock(&bcache.bufmap_lock, "bufmap_lock");
}

//查找对应dev、blockno的缓存块。若有则返回；若无则找到一块最近最久未使用的空缓存，建立映射
//但是该函数不负责将对应磁盘块的内容读入缓存
// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  uint bucket;

  //计算哈希函数，即所在桶
  bucket = BUFMAP_HASH(dev, blockno);

  acquire(&bcache.bufmap_bucket_lock[bucket]); //获取当前桶锁

  //如果桶中有对应缓存
  // Is the block already cached?
  for(b = &bcache.bufmap[bucket]; b != 0; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++; //引用计数加1
      release(&bcache.bufmap_bucket_lock[bucket]); //释放当前桶锁
      acquiresleep(&b->lock); //获取缓存块睡眠锁
      return b;
    }
  }

  //如果桶中没有对应缓存，需要从别的桶中偷取空的缓存块
  // Not cached.

  release(&bcache.bufmap_bucket_lock[bucket]); //释放当前桶锁

  acquire(&bcache.bufmap_lock); //获取哈希表锁，使得对哈希表中所有桶的内容修改是线性的

  //再次检查同种是否有对应缓存，原因是在释放桶锁和获取哈希表锁之间的时间可能会有缓存变化
  for(b = &bcache.bufmap[bucket]; b != 0; b = b->next){
      if(b->dev == dev && b->blockno == blockno){
          acquire(&bcache.bufmap_bucket_lock[bucket]);
          b->refcnt++;
          release(&bcache.bufmap_bucket_lock[bucket]);
          release(&bcache.bufmap_lock);
          acquiresleep(&b->lock);
          return b;
      }
  }

  //为了便于将目标buf从原桶摘出，设置一个指向目标的前一个结点的指针
  struct buf *LRU_buf_pre = 0;
  //表示目标当前所在的桶
  uint holding_bucket = -1;
  //遍历各桶，寻找空的缓存块
  for(int i = 0; i < BUFMAP_SIZE; i++){
      acquire(&bcache.bufmap_bucket_lock[i]); //获取目标桶锁
      int find = 0; //find为0表示没有在该桶中发现更小lastuse的空缓存，即没有在该桶中更新LRU_buf_pre
      //遍历桶中的所有缓存块
      for(b = &bcache.bufmap[i]; b->next; b = b->next){
          //找到一个结点的next结点为空，且此时还没有找到一个符合条件的buf或者这个buf比之前所找buf的lastuse更小
          //更小意味着在更长的时间里没有被更新过，lastuse表示的是更新时间，越小就越久没有更新，更符合要求
          //将LRU_buf_pre指向这个结点，这个节点的next就是目标空缓存
          if(b->next->refcnt == 0 && (!LRU_buf_pre || b->next->lastuse < LRU_buf_pre->next->lastuse)){
              LRU_buf_pre = b;
              find = 1; //find为1表示在桶中发现了更小lastuse的空缓存,即在该桶中更新了LRU_buf_pre
          }
      }

      if(!find) //如果该桶中没有更小的lastuse的空缓存
          release(&bcache.bufmap_bucket_lock[i]); //直接释放该桶锁
      else {
          if(holding_bucket != -1) 
          //否则，说明这个桶里有符合要求（LRU）的空缓存，那么释放之前被认为保存目标的桶的桶锁（除非之前还没有找到过目标，即holding_bucket=-1）
              release(&bcache.bufmap_bucket_lock[holding_bucket]);

          //然后将保存目标的桶更新
          holding_bucket = i;             
      }
  }
  
  //没有空缓存了，报错
  if(!LRU_buf_pre)
      panic("bget: no buffers");

  //LRU_buf_pre->next即为目标buf
  b = LRU_buf_pre->next;

  //若本桶与目标桶不同，就从目标桶偷取目标buf
  if(holding_bucket != bucket){
      LRU_buf_pre->next = b->next; //摘下目标buf
      release(&bcache.bufmap_bucket_lock[holding_bucket]); //释放目标桶锁
      acquire(&bcache.bufmap_bucket_lock[bucket]);  //接下来要将目标buf插入本桶，所以要获取本桶桶锁
      b->next = bcache.bufmap[bucket].next;
      bcache.bufmap[bucket].next = b;
  }

  b->dev = dev;
  b->blockno = blockno;
  b->refcnt = 1;
  b->valid = 0;
  release(&bcache.bufmap_bucket_lock[bucket]);
  release(&bcache.bufmap_lock);
  acquiresleep(&b->lock);
  return b;
}

//将磁盘块内容读入与之建立映射的缓存块中
// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno); //找到与之建立映射的缓存块
  if(!b->valid) {
    virtio_disk_rw(b, 0); //读入数据
    b->valid = 1;
  }
  return b;
}

//将缓存块的数据写回磁盘块
// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock)) //检查是否持有睡眠锁
    panic("bwrite");
  virtio_disk_rw(b, 1); //写回
}

//释放缓存块
// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint bucket = BUFMAP_HASH(b->dev, b->blockno);

  acquire(&bcache.bufmap_bucket_lock[bucket]);
  b->refcnt--; //使用该缓存块的进程减1
  if (b->refcnt == 0) { 
    // no one is waiting for it.
    b->lastuse = ticks; //更新使用时间为当前ticks
  }
  release(&bcache.bufmap_bucket_lock[bucket]);
}

void
bpin(struct buf *b) {
  uint bucket = BUFMAP_HASH(b->dev, b->blockno);

  acquire(&bcache.bufmap_bucket_lock[bucket]);
  b->refcnt++;
  release(&bcache.bufmap_bucket_lock[bucket]);
}

void
bunpin(struct buf *b) {
  uint bucket = BUFMAP_HASH(b->dev, b->blockno);

  acquire(&bcache.bufmap_bucket_lock[bucket]);
  b->refcnt--;
  release(&bcache.bufmap_bucket_lock[bucket]);
}


