# 高并发内存池--tcmalloc

#### 介绍
当前项⽬是实现⼀个⾼并发的内存池，他的原型是google的⼀个开源项⽬tcmalloc，tcmalloc全称
Thread-Caching Malloc，即线程缓存的malloc，实现了⾼效的多线程内存管理，⽤于替代系统的内
存分配相关的函数（malloc、free）

#### 知识储备
这个项⽬会⽤到C/C++、数据结构（链表、哈希桶）、操作系统内存管理、单例模式、多线程、互斥锁
等等⽅⾯的知识

#### 软件架构
TCMalloc 核心采用三级缓存架构，从线程私有到全局共享分层设计，最大化减少多线程锁竞争，核心架构分为三层：
​​​​​​![输入图片说明](image.png)
用户层
  ├── ConcurrentAlloc / ConcurrentFree
  │
ThreadCache（线程私有）
  ├── 无锁自由链表
  ├── 慢启动批量获取
  ├── 超阈值自动回收至 CentralCache
  │
CentralCache（全局中心缓存）
  ├── 分桶细粒度锁（桶锁）
  ├── 管理多个 span
  ├── 内存块归位、span 空闲判定
  │
PageCache（页缓存）
  ├── 全局页锁
  ├── 向系统申请/释放内存
  ├── span 拆分、前后页合并
  ├── 地址 ↔ pageId ↔ span 映射

#### 文件结构
ConcurrentMemoryPool/
├── Common.h              // 公共定义、宏、工具函数、内存对齐、页大小
├── Fixed-size_memory_pool.h  // 定长对象池（管理span/ThreadCache）
├── ThreadCache.h/cpp     // 线程缓存：无锁快速分配
├── CentralCache.h/cpp    // 中心缓存：桶锁、span管理
├── PageCache.h/cpp       // 页缓存：页分配、span合并、系统调用
├── ConcurrentAlloc.h     // 用户接口：ConcurrentAlloc / ConcurrentFree
├── Benchmark.cpp         // 多线程性能测试
├── UnitTest.cpp          // 单元测试
└── README.md

#### 运行方式
Windows（VS 直接编译）
新建空项目
添加所有头文件 / 源文件
直接编译运行
运行 Benchmark.cpp 查看多线程性能对比
Linux（可扩展）
已预留 mmap/brk 实现位置，补充后可跨平台编译运行
