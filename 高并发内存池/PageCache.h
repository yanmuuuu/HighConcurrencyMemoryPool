#pragma once
#include "Common.h"
#include "Fixed-size_memory_pool.h"

class PageCache
{
public:
	static PageCache* GetInstance()
	{
		return &_sInst;
	}

	span* NewSpan(size_t size);				//在PageList里面找有没有符合要求的_freeList的节点，没有就向系统申请一个大块内存

	span* MapObjectToSpan(void* obj);		//获取从对象到span的映射

	void ReleaseSpanToPageCache(span* obj); //将CentralCache里面的span还回PageCache

private:
	spanList _spanlist[MAX_PAGE_BUCKETS];   //定义128个桶
	ObjectPool<span> spanPool;
	std::unordered_map<PAGE_ID, span*> _idSpanMap;
public:
	std::mutex _pageMutex;					//定义一个大锁而不是桶锁
private:
	PageCache()
	{ }
	PageCache(const PageCache&) = delete;
	static PageCache _sInst;
};