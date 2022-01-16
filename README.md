# Lab: Multithreading

[**Lab Assignment**](https://pdos.csail.mit.edu/6.828/2020/labs/thread.html)

## Hint
1. 某个线程第一次运行时，让他运行传递给thread_create的函数，on it's own stack。（在context里面设置ra和sp寄存器)
2. 保证 thread_switch 保存好和重新加载好那些寄存器。思考这些寄存器应该被保存在哪里
3. 修改struct thread 是个不错的主意