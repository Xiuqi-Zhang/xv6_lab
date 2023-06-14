struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  uint lastuse; //用于寻找LRU缓存
  struct buf *next; 
  uchar data[BSIZE];
};

