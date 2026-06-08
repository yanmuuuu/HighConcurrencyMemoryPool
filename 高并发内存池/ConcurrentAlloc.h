#pragma once
#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"
#include "Fixed-size_memory_pool.h"

//用户层

static void* ConcurrentAlloc(size_t size)			//用户接口--申请内存
{
	//如果申请的内存 <= 256k
	//正常申请
	//如果申请的内存 > 256k
	//分为两种情况
	//1.因为我们最多有128(MAX_PAGE_BUCKETS)页，所以
	//如果申请内存在256k < .. <= 128 * 8k，可以直接去pageCache申请
	//2.如果比128 * 8k还大，那么直接向系统申请
	if (size > MAX_BYTES)
	{
		size_t alignSize = SizeClass::RoundUp(size); //对齐数
		size_t kPages = alignSize >> PAGE_SHIFT; //页数
		PageCache::GetInstance()->_pageMutex.lock();
		span* span = PageCache::GetInstance()->NewSpan(kPages); //申请一个这么多页数的span
		span->_objSize = size;
		PageCache::GetInstance()->_pageMutex.unlock();
		
		void* ptr = (void*)(span->_pageId << PAGE_SHIFT); //将id * 8k就得到了span的起始地址
		return ptr;
	}
	else
	{
		if (pTLSThreadCache == nullptr)
		{
			static ObjectPool<ThreadCache> tcPool;
			pTLSThreadCache = tcPool.New();
		}
		//std::cout << std::this_thread::get_id() << ":" << pTLSThreadCache << std::endl;
		return pTLSThreadCache->Allocate(size);
	}	
}

//传入size的写法
/*
static void ConcurrentFree(void* ptr, size_t size)  //用户接口--释放内存
{
	assert(ptr);
	if (size > MAX_BYTES)
	{
		span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
		PageCache::GetInstance()->_pageMutex.lock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->_pageMutex.unlock();
	}
	else
	{
		assert(pTLSThreadCache);
		pTLSThreadCache->Deallocate(ptr, size);
	}
}*/

//不传入size的写法
static void ConcurrentFree(void* ptr)  //用户接口--释放内存
{
	assert(ptr);
	span* obj = PageCache::GetInstance()->MapObjectToSpan(ptr);
	size_t size = obj->_objSize;
	if (size > MAX_BYTES)
	{
		PageCache::GetInstance()->_pageMutex.lock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(obj);
		PageCache::GetInstance()->_pageMutex.unlock();
	}
	else
	{
		assert(pTLSThreadCache);
		pTLSThreadCache->Deallocate(ptr, size);
	}
}


