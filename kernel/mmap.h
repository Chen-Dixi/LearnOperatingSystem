struct vma_struct {
  int valid;
  struct file* fp;
  uint64 addr; // addr 为0表示这个vma元素对象没有被占用
  int length;
  int prot; // PROT_READ | PROT_WRITE
  int flags; // MAP_SHARED | MAP_PRIVATE
  int offset; // 表示这段vma的addr在映射文件中的偏移
  struct vma_struct *next, *prev;
};