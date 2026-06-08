#pragma once
#include "Common.h"

class ThreadCache
{
public:
	void* Allocate(size_t size);                              //申请内存对象

	void Deallocate(void* ptr, size_t size);                  //释放内存对象

	void* FetchFromCentralCache(size_t index, size_t size);   //向中心缓存获取对象

	void ListTooLong(FreeList& list, size_t size);			  //释放对象的时候发现链表过长，就回收内存到中心缓存
private:
	FreeList _freeList[MAX_BUCKETS];						  //使用我们的分组方案，只需要分208组，就可以表示256kb以下情况
};

//_declspec(thread) 是MSVC(Windows)专属语法，标记变量为线程私有
//线程局部存储(TLS):_declspec(thread) 是为了让 pTLSThreadCache 成为线程私有变量，每个线程有独立副本，实现 ThreadCache 无锁访问
//个人解释:
//没有 _declspec(thread) 的问题：
//如果直接写 static ThreadCache* pTLSThreadCache = nullptr; 
//这个指针是全局共用的——所有线程都用这一个指针指向同一个 ThreadCache（内存池）
//多线程同时分配 / 释放内存时，就得加锁保护这个内存池，一锁就慢，高并发下性能直接垮掉（这也是系统 malloc 慢的原因之一）。
//加了_declspec(thread) 的好处：
//这个关键字就是告诉编译器：“给每个线程都单独复制一份 pTLSThreadCache 指针，每个线程的指针只归自己用”
//比如线程 A 有自己的 pTLSThreadCache，指向线程 A 专属的内存池；线程 B 有自己的 pTLSThreadCache，指向线程 B 专属的内存池
//线程 A 和 B 操作自己的内存池时，完全不用管对方，不用加锁，速度直接拉满（这就是 TCMalloc “高并发” 的核心）
//*************************
//一句话，无锁操作就是让每个线程先用地盘专属的小内存池（ThreadCache），不用抢全局大池，所以不用加锁
//只有小池空了，才去全局大池批量拿内存（仅锁对应桶），整体几乎无锁
static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;
