# Lab: locks

测试里面经常调用 kalloc 和 kfree。
acquire 里面的 循环次数可以用来粗略地衡量 锁的密集程度。

## Spinlock

### acuire & release
`acquire` 利用到一个 amoswap 原子操作
```c++
while(__sync_lock_test_and_set(&lk->locked, 1) != 0)
```

`release` 用到`__sync_lock_release`
`__sync_lock_release`和`__sync_lock_test_and_set`都是 C语言的一个库函数

acquire 里面的 循环次数可以用来粗略地衡量 锁的密集程度。
* push_off before holding the lock
* pop_off after releasing the lock
**⚠️ 持有spinlock 时不能yield，此时必须关中断。**
```c++
// push_off/pop_off are like intr_off()/intr_on() except that they are matched:
// it takes two pop_off()s to undo two push_off()s. Also, if interrupts
// are initially off, then push_off, pop_off leaves them off.
void
push_off(void)
{
    int old = intr_get();

    intr_off();
    if(mycpu()->noff == 0)
        mycpu()->intena = old;
    mycpu()->noff += 1;
}


void
pop_off(void)
{
    struct cpu *c = mycpu();
    if(intr_get())
        panic("pop_off - interruptible");
    if(c->noff < 1)
        panic("pop_off");
    c->noff -= 1;
    if(c->noff == 0 && c->intena)
        intr_on();
}
```

## Sleep Locks
1. 持有spinlock锁的进程在执行一个需要很长时间的任务时，其他CPU的进程想要获取这个锁只能忙等(while loop) ，浪费CPU不停自旋
2. 希望持有锁的进程，在等待Disk的时候能让出CPU（持有spinlock的进程不会让出CPU）

Sleeplock 能让出CPU，允许其他线程去再次acquire它。
1. 因为Sleeplock 允许中断，所以不能在中断处理程序中使用Sleeplock
2. Spinlock 里面不能使用Sleeplock