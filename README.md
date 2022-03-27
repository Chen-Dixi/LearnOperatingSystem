# Lab: [File System](https://pdos.csail.mit.edu/6.828/2020/labs/fs.html)

## Tips
- 还需要修改 `MAXFILE(kernel/fs.h)` 
- create(kernel/sysfile.c)返回的是上锁的inode，在`sys_symlink`里避免重复上锁