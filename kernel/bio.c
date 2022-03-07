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


void put(uint blockno, struct buf *b);

struct bufbucket {
  struct buf head;       // 头节点
  struct spinlock lock;  // 锁
};

struct {
  struct spinlock lock;
  
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
  
  // hashmap
  struct bufbucket tab[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;
  char lockname[16];

  initlock(&bcache.lock, "bcache");
  for (int i=0; i<NBUCKET;i++){
    snprintf(lockname, sizeof(lockname), "bcache.%d", i);
    initlock(&bcache.tab[i].lock, "bcache.bucket");
    // 初始化散列桶的头节点
    bcache.tab[i].head.prev = &bcache.tab[i].head;
    bcache.tab[i].head.next = &bcache.tab[i].head;
  }
  /*
  // Create linked list of buffers
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }*/

  // remove list of all buffers
  // NBUF == 30

  for(int i = 0; i < NBUF; i++){
    b = &bcache.buf[i];
    initsleeplock(&b->lock, "buffer");
    // put(i, b);
    acquire(&tickslock);
    b->lru_timestamp = ticks;
    release(&tickslock);
    put(i, b);
  }
}

void
put(uint blockno, struct buf *b){
  uint i = (NBUCKET-1) & blockno;
  
  b->next = bcache.tab[i].head.next;
  b->prev = &bcache.tab[i].head;
  
  bcache.tab[i].head.next->prev = b;
  bcache.tab[i].head.next = b;
  // record the bucket id of the buffer
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  uint bucket_id = (NBUCKET-1) & blockno; // bucket index

  // acquire(&bcache.lock);

  // Is the block already cached?
  /*
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  } */
  // Is the block already cached?
  acquire(&bcache.tab[bucket_id].lock);
  for(b = bcache.tab[bucket_id].head.next; b != &bcache.tab[bucket_id].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      
      acquire(&tickslock);
      b->lru_timestamp = ticks;
      release(&tickslock);

      release(&bcache.tab[bucket_id].lock);
      // release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  /*
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0) { // not in use
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }*/
  // choose a lru_buffer
  struct buf* lru_b = 0;
  b = 0;
  // 每次从bucket 里面取
  for(int try_bucket_id = bucket_id, cycle = 0; cycle < NBUCKET ;try_bucket_id = (try_bucket_id+1) & (NBUCKET - 1), cycle++){
    b = 0;
    if(try_bucket_id != bucket_id){
      acquire(&bcache.tab[try_bucket_id].lock);
    }

    for(lru_b = bcache.tab[try_bucket_id].head.next; lru_b != &bcache.tab[try_bucket_id].head; lru_b=lru_b->next){
      if(lru_b->refcnt == 0 && (b == 0 || lru_b->lru_timestamp < b->lru_timestamp))
        b = lru_b;
    }

    if(b){
      // 从链表里取出来
      b->next->prev = b->prev;
      b->prev->next = b->next;

      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1; // prevent buffer from being re-used for a different disk block
      
      acquire(&tickslock);
      b->lru_timestamp = ticks;
      release(&tickslock);

      put(blockno, b);
      if(try_bucket_id != bucket_id)
        release(&bcache.tab[try_bucket_id].lock);
      release(&bcache.tab[bucket_id].lock);
      // release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }else{
      if(try_bucket_id != bucket_id)
        release(&bcache.tab[try_bucket_id].lock);
    }
  }
  
  panic("bget: no buffers");
}


// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");
  int bucket_id = (NBUCKET-1) & b->blockno;
  releasesleep(&b->lock);
  // acquire(&bcache.lock);
  acquire(&bcache.tab[bucket_id].lock);
  
  b->refcnt--;
  // if (b->refcnt == 0) {
  //   no one is waiting for it.
  //   b->next->prev = b->prev;
  //   b->prev->next = b->next;
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }
  acquire(&tickslock);
  b->lru_timestamp = ticks;
  release(&tickslock);

  release(&bcache.tab[bucket_id].lock);
  // release(&bcache.lock);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


