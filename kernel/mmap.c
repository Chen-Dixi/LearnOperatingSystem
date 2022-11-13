#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "proc.h"
#include "defs.h"
#include "fcntl.h"

struct {
  struct spinlock lock;
  struct vma vma[NVMA];
} vmatable;


// 从整个系统的vmatbale中选一个还没被占用的vma，返回给他进行记录
// va 是 page-aligned
struct vma*
vmaalloc()
{
  struct vma* vma;
  acquire(&vmatable.lock);
  for(vma = vmatable.vma; vma < vmatable.vma + NVMA; vma++) {
    if (vma->valid == 0) {
      vma->valid = 1;
      release(&vmatable.lock);    
      return vma;
    }
  }

  release(&vmatable.lock);
  return 0;
}

void vmaclose(struct vma *vma) {
  
}

// 每一页内存，全部用来映射同一文件。
// return -1 on error
uint64
mmap(struct file* f, int length, int prot, int flags, int offset)
{
  uint64 sz;
  struct proc *p = myproc();
  uint64 oldsz = p->sz;

  oldsz = PGROUNDUP(oldsz);
  if ((prot & PROT_WRITE) && (flags & MAP_SHARED) && !f->writable) {
    // 不可写的文件，不能以PROT_WRITE和 MAP_SHARED方式进行映射
    return -1;
  }

  // 页表项标志位 PTE_FLAG
  // 是否可写，是否可执行，是否可读
  int perm = (prot & PROT_WRITE ? PTE_W : 0) | (prot & PROT_EXEC ? PTE_X : 0) |  (prot & PROT_READ ? PTE_R : 0);
  
  // 映射一个文件，懒分配一整块内存页作为mmap区域。 PTE_MMAP表示这一页内存都是用来做mmap的，懒分配时，perm不包含PTE_V
  if ((sz = uvmalloc_lazy(p->pagetable, oldsz, oldsz + length, PTE_MMAP|perm|PTE_U)) == 0) {
    return 0;
  }

  p->sz = PGROUNDUP(sz); // All 4096 Bytes of a page should be used for one mmaped-file。

  return oldsz;
}

// ip: inode pointer
// 从vma的文件中读取一整个page，拷贝到va所在的地址。内存中应该保留一整页的数据
int
mmapread(struct vma* vma, uint64 va)
{
  uint64 va_down;
  uint off;
  struct file* f = vma->fp;
  va_down = PGROUNDDOWN(va);
  
  off = va_down - vma->addr + vma->offset;
  ilock(f->ip);
  // 读一整个page
  if (readi(f->ip, 1, va_down, off, PGSIZE) < 0) {
    iunlock(f->ip);
    return -1;
  }
  iunlock(f->ip);
  return 0;
}

// 更新vma->addr 或 vma->length
// addr 和 length 不需要满足 page-aligned
int
munmap(struct vma* vma, uint64 addr, int length) {
  printf("addr: %d | vma_addr: %d, vma_length %d\n", addr, vma->addr, vma->length);
  // vma->valid现在是1，不会被其他进程使用。lab没有线程，无线程安全问题
  uint64 vma_addr = vma->addr;
  int vma_length = vma->length;
  uint64 vma_end = vma_addr + vma_length - 1; // vma范围内最后一个字节地址
  uint64 end = addr + length - 1;
  uint64 a, last;
  pte_t* pte;
  struct proc* p = myproc();
  
  // 1 边界判断
  // if (addr + (uint64)length < addr)
  //   panic("munmap: uint64 overflow");

  if (addr < vma_addr || (addr > vma_addr && end < vma_end) || addr > vma_end || end > vma_end){
    printf("addr: %d, end: %d| vma_addr: %d, vma_end %d\n", addr, end, vma_addr, vma_end);
    // punch a hole in the middle of the mmaped-region
    panic("munmap: invalid addr range");
  }
  
  // 一页一页地 munmap
  // addr ~ addr + length 有可能在vma的头 或者 尾部
  if (addr == vma_addr) {
    // unmap at the start
    a = PGROUNDDOWN(addr);
  } else {
    // unmap at the end
    a = PGROUNDUP(addr);
  }

  if (end == vma_end){
    last = PGROUNDDOWN(end);
  } else {
    if (PGROUNDDOWN(end) != PGROUNDDOWN(vma_end)) {
      last = PGROUNDDOWN(end);
    } else {
      last = PGROUNDDOWN(end) - PGSIZE;
    }
  }
  
  
  for(;;) {
    if(a > last)
      break;
    
    if((pte = walk(p->pagetable, a, 0)) == 0){
      panic("munmap: walk");
    }

    if((*pte & PTE_D) && (*pte & PTE_MMAP) && vma->flags == MAP_SHARED) {
      // 把这一页写回去
      if (vmawrite(vma, a, PGSIZE)!=PGSIZE){
        // 没有写回一整页
        panic("munmap: vmawrite");
      }
    }
    // 尝试释放内存
    // 释放1整页，do free
    uvmunmap(p->pagetable, a, 1, 1);
    a += PGSIZE;
  }
  // addr ~ addr + length 有可能在vma的头 或者 尾部
  if (addr == vma_addr) {
    // unmap at the start
    vma->addr = addr + length;
    vma->offset += length;
    vma->length = vma->length - length;
  } else {
    // unmap at the end
    vma->length = addr - vma_addr;
  }

  if (vma->length == 0) {
    fileclose(vma->fp);
    
    
    acquire(&vmatable.lock);
    
    p->mappedvma[vma->vmad] = 0;
    vma->valid = 0;
    vma->fp = 0;
    vma->addr = 0;
    vma->flags = 0;
    vma->prot = 0;
    vma->length = 0;
    vma->offset = 0;
    vma->vmad = 0;
    release(&vmatable.lock);
  }
  return 0;

}

int
vmadalloc(struct proc* p, struct vma* vma)
{
  int vmad;

  for(vmad = 0; vmad < NVMA; vmad++) {
    if (p->mappedvma[vmad] == 0) {
      p->mappedvma[vmad] = vma;
      return vmad;
    }
  }
  return -1;
}

void proc_freemap(struct proc* p) {
  for(int i=0; i<NVMA; i++) {
    if (p->mappedvma[i] && p->mappedvma[i]->valid) {
      munmap(p->mappedvma[i], p->mappedvma[i]->addr, p->mappedvma[i]->length);
    }
  }
}

int mmapcopy(struct proc* p, struct proc* np)
{
  struct vma* v;
  int vmad;
  for(int i=0; i<NVMA; i++) {
    if (p->mappedvma[i] && p->mappedvma[i]->valid) {
      // 拷贝
      if ((v = vmaalloc()) == 0 || (vmad = vmadalloc(np, v)) < 0) {
        if (v)
          v->valid = 0;
        goto err;
      }
      v->addr = p->mappedvma[i]->addr;
      v->fp = p->mappedvma[i]->fp;
      v->flags = p->mappedvma[i]->flags;
      v->prot = p->mappedvma[i]->prot;
      v->length = p->mappedvma[i]->length;
      v->offset = p->mappedvma[i]->offset;
      v->vmad = vmad;
      v->fp = filedup(p->mappedvma[i]->fp);
    }
  }
  return 0;

err:
  proc_freemap(np);
  return -1;
}