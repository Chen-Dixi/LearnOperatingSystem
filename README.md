# Lab: Copy-on-Write

## [Lab Assignment](https://pdos.csail.mit.edu/6.828/2020/labs/cow.html)

![Tx9VTf.png](https://s4.ax1x.com/2022/01/06/Tx9VTf.png)

1. uvmcopy 把父进程的物理内存映射给子进程。父子进程的PTE_W 都去掉
2. 在usertrap里面捕获 page_fault。开辟 物理内存页，把旧的物理内存页里面的东西 拷贝到 新的物理内存页里面去。kallco 创建一个物理page。(cow，是一次拷贝一个page，并不需要将整个子进程的page都拷贝一份)
3. 每个物理page，当 引用计数都降为0过后，记得free
    1. 每个page 有自己的计数
    2. kalloc的时候reference count设为1；fork过后，page被子进程共享，reference count+1；每当一个进程不再需要这个page时，reference count - 1
4. copyout 里面要处理 cow page。