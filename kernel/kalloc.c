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
struct run* steal_freelist();
extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct kernelmem{
  struct spinlock lock;
  struct run* freelist;
} kmems[NCPU];

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

// allocate physical page for cpu 0
void
kinit_0()
{
  initlock(&kmems[0].lock, "kmem"); // TBD, string formatting
  freerange(end, (void*)PHYSTOP);
}

void kinit_per_cpu()
{
  initlock(&kmems[cpuid()].lock,"kmem"); // don't need push_off() here, it is uninterruptible now.
  kmems[cpuid()].freelist=0;
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
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
  struct kernelmem* cmem;
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);
  
  r = (struct run*)pa;
  push_off(); // for cpuid;
  cmem = &kmems[cpuid()];
  
  acquire(&cmem->lock);
  r->next = cmem->freelist;
  cmem->freelist = r;
  release(&cmem->lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  struct kernelmem *cmem;
  push_off();
  cmem = &kmems[cpuid()];
  acquire(&cmem->lock);
  r = cmem->freelist;
  if(!r) // one CPU must "steal" part of the other CPU's free list
    r = steal_freelist();
  
  if(r)
    cmem->freelist = r->next;
  
  release(&cmem->lock);
  pop_off(); // TBD maybe we can put this line of code a little more ahead
  
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

struct run*
steal_freelist(void){
  struct run *r = 0;
  struct kernelmem *cmem;
  int cid = cpuid();
  int found = 0;
  struct run *quick;
  struct run *slow;

  for(int i=0; i < NCPU; i++){
    
    if (i==cid)
      continue;
    cmem = &kmems[i];
    acquire(&cmem->lock);
    r = cmem->freelist;
    // quick and slow pointer
    if(r){
      found = 1;
      quick = r;
      slow = r;
      while(quick && quick->next){
        quick = quick->next->next;
        slow = slow->next;
      }
      cmem->freelist = slow->next;
      slow->next=0;
    }
    release(&cmem->lock);
    if(found)
      break;
  }
  return r;
}
