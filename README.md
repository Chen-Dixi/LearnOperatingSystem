# Lab: locks

测试里面经常调用 kalloc 和 kfree。
acquire 里面的 循环次数可以用来粗略地衡量 锁的密集程度。