# Note
| | |
| --- | --- |
| mmap_test() | 1 mmap只读的文件，不能使用PROT_WRITE指令  2 可以写回文件 3 可以一点点的unmap 4 可同时映射2文件 |
| mmap_test()调用unlink |  |
| mmap的几个参数 | length 的长度不一定和 文件的大小相同。|
| PROT |0: None; 1: PROT_READ; 2: PROT_WRITE; 4: PROT_EXEC |
| struct file 结构体里面 读写权限 | char writable = (omode & O_WRONLY) \|\| (omode & O_RDWR); char readable = !(omode & O_WRONLY);|
| vm.c include file.h 失败 | 提示 file.h里面的inode ，sleeplock lock 成员找不到， 解决办法是在 vm.c 引用的头文件中加入 include “sleeplock.h” |
| 内存页异常里加处理函数 | 从进程的 vma 记录里面获取 file pointer，读文件里的数据；在进程退出的函数中是不是也要清理进程里面 vma的东西。✅ |
| proc.c 里可以有 stuct file，为什么没有 struct vma | file 其实也用不了，只要在 proc.c 里面，f→ 尝试获取里面的成员，就不能编译，c文件可以f→获取成员的地方，一定include 完成的结构类型 |
| 根据记录的 memory area结构，将文件数据内容拷贝出去 |  |