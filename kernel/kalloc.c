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

// 全局变量，是不是不用担心分配问题
// TODO 注意是否需要加锁，or copy-on-write🤔
int mem_rcount[PHYSTOP>>12];

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  printf("mem_rcount[0]: %d, mem_rcount[1]:%d, mem_rcount len:%d, mem_rcount address:%p\n", mem_rcount[0],mem_rcount[1], sizeof(mem_rcount), mem_rcount);
  printf("Now, the end address: %p\n",pa_start);
  p = (char*)PGROUNDUP((uint64)pa_start);
  printf("The pa_start memory address: %p\n",p);
  // set all reference count to zero
  memset(mem_rcount, 0, sizeof(mem_rcount));

  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  
  // decrement reference count
  decmemrefcount(pa);
  // should only place a page back on the free list if its reference count is zero
  if(getrefcount(pa) > 0)
    return;
  
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

void
decmemrefcount(void* pa)
{
  uint64 index = PA2INDEX(pa);
  acquire(&kmem.lock);
  mem_rcount[index] = mem_rcount[index] - 1;
  release(&kmem.lock);
}

void
incmemrefcount(void* pa)
{
  acquire(&kmem.lock);
  uint64 index = PA2INDEX(pa);
  mem_rcount[index] = mem_rcount[index] + 1;
  release(&kmem.lock);
}

int
getrefcount(void* pa)
{
  int refcount;
  acquire(&kmem.lock);
  uint64 index = PA2INDEX(pa);
  refcount = mem_rcount[index];
  release(&kmem.lock);
  return refcount;
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
  if(r){
    kmem.freelist = r->next;
    // Set a page's reference count to one when kalloc() allocates it
    mem_rcount[PA2INDEX(r)] = 1;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
