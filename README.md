# -
当前项⽬是实现⼀个⾼并发的内存池，他的原型是google的⼀个开源项⽬tcmalloc，tcmalloc全称 Thread-Caching Malloc，即线程缓存的malloc，实现了⾼效的多线程内存管理，⽤于替代系统的内 存分配相关的函数（malloc、free）
