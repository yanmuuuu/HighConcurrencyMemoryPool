#include "CentralCache.h"
#include "PageCache.h"

CentralCache CentralCache::_sInst;

// 获取⼀个非空的span
span* CentralCache::GetOneSpan(spanList& list, size_t size)
{
	span* it = list.Begin();
	while (it != list.End())
	{
		if (it->_freeList != nullptr)
		{
			return it;
		}
		it = it->_next;
	}
	//这里有两种解锁方法
	//1.把锁带入NewSpan()中
	//2.现在就解锁
	//我们选择2，如果只考虑申请，两种锁都可以，但是如果释放呢，那么第一种锁会让释放的线程拿不到锁而阻塞
	//而第二种因为已经把锁释放了，所以可以完成线程释放
	list._mutex.unlock();

	//在外部加入page全局锁
	PageCache::GetInstance()->_pageMutex.lock();
	//走到这里说明没有非空span，需要向pageCache申请了
	span* newspan = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	newspan->_isUse = true;
	newspan->_objSize = size;
	//完成函数解锁
	//为什么切分的时候不需要这个锁呢，因为此时线程切分的内容只有自己访问得到，不涉及共享资源，所以不需要加锁
	PageCache::GetInstance()->_pageMutex.unlock();

	//计算span的大块内存起始地址以及内存大小(字节数) -> 加起来就是结束地址
	//这里用char* 而不是void* 是因为char* 更适合字节加减(+1就是一字节)
	char* start = (char*)(newspan->_pageId << PAGE_SHIFT); //相当于 * 1024 * 8 (8k)
	size_t bytes = newspan->_n << PAGE_SHIFT;
	char* end = start + bytes;

	//把大块内存切成小块内存自由链表挂载到span->_freeList中
	//1.先切一个小块作为头节点
	newspan->_freeList = start;
	start += size;
	void* tail = newspan->_freeList;
	//2.切剩下的，尾插(也可以使用头插，不过尾插可以保证物理内存线性连续)
	while (start < end)
	{
		//尾插
		NextObj(tail) = start;
		tail = NextObj(tail); //tail = start;
		//后移
		start += size;
	}
	NextObj(tail) = nullptr;
	//会访问共享资源,eg:
	//线程一找list里面有没有span，而线程儿二正在插入span
	//所以加上桶锁
	list._mutex.lock();
	list.Push_front(newspan); //头插到list中

	return newspan;
}

// 从中⼼缓存获取⼀定数量的对象给thread cache
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
{
	size_t index = SizeClass::Index(size);
	//加上桶锁避免线程竞争(互斥)
	_spanList[index]._mutex.lock();
	span* span = GetOneSpan(_spanList[index], size);
	assert(span);
	assert(span->_freeList);
	start = span->_freeList;
	end = start;
	size_t i = 0;
	size_t actualNum = 1;
	while (i < batchNum - 1 && NextObj(end) != nullptr)
	{
		end = NextObj(end);
		i++;
		actualNum++;
	}
	span->_freeList = NextObj(end);
	NextObj(end) = nullptr;
	span->_useCount += actualNum;

	_spanList[index]._mutex.unlock();
	return actualNum;
}

//将一定数量的对象释放到span跨度
//为什么要给字节数呢，当然是要算在哪一个桶
//但是我们的start里面可能存在多个span的节点，所以要用到哈希表让每一个节点精准释放到size映射的桶里面
void CentralCache::ReleaseListToSpans(void* start, size_t size)
{
	size_t index = SizeClass::Index(size);
	_spanList[index]._mutex.lock();
	while (start)
	{
		void* next = NextObj(start);

		span* span = PageCache::GetInstance()->MapObjectToSpan(start); //获取从对象到span的映射

		//将对应映射的start内存头插还给span
		NextObj(start) = span->_freeList;
		span->_freeList = start;

		span->_useCount--;

		//说明span的切分出去的所有块都已经还回来了，可以返还给PageCache了
		if (span->_useCount == 0)
		{
			_spanList[index].Erase(span);
			span->_freeList = nullptr;
			span->_next = nullptr;
			span->_prev = nullptr;
			_spanList[index]._mutex.unlock();
			PageCache::GetInstance()->_pageMutex.lock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
			PageCache::GetInstance()->_pageMutex.unlock();
			_spanList[index]._mutex.lock();
		}

		start = next;
	}
	_spanList[index]._mutex.unlock();
}