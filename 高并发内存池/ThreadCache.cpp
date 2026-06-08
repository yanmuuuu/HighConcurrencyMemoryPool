#include "ThreadCache.h"
#include "CentralCache.h"

//向中心缓存(central cache)申请内存对象
//申请到多个就先挂载(start->next到end, 因为start是要申请的内存结点)
//最终都返回start这个节点(一个节点，即申请的内存)
void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	assert(index < MAX_BUCKETS);
	assert(size <= MAX_BYTES);
	//满开始反馈调节算法
	//最开始申请就给一，之后_freeList[index].GetMaxSize()++
	//最次也会保证即增长的同时也不会一次给太多
	size_t batchNum = min(_freeList[index].GetMaxSize(), SizeClass::NumMoveSize(size)); //这里使用的是_WIN32下的宏min，而不是algorithm
	if (batchNum == _freeList[index].GetMaxSize())
	{
		_freeList[index].GetMaxSize()++;
	}

	void* start = nullptr;
	void* end = nullptr;
	// 从中心缓存获取一定数量的对象给thread cache
	size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, size);
	assert(actualNum > 0);

	if (actualNum == 1) //只申请了一个，直接返回就可以
	{
		assert(start == end);
		return start;
	}
	else //申请了多个，就需要先将[NextObj(start), end]挂到_freeList[index]中，再返回start
	{
		_freeList[index].rangePush(NextObj(start), end, actualNum - 1);
		return start;
	}
}

void* ThreadCache::Allocate(size_t size)             //申请内存
{
	size_t align = SizeClass::RoundUp(size);         //对齐
	size_t index = SizeClass::Index(size);		     //映射
	if (_freeList[index].empty())				     //为空，向中心缓存(central cache)申请内存对象
	{
		return FetchFromCentralCache(index, align);
	}
	else										     //有的话直接返回节点就行
	{
		return _freeList[index].pop();
	}
}

void ThreadCache::Deallocate(void* ptr, size_t size) //释放内存
{
	assert(ptr);
	assert(size <= MAX_BYTES);
	size_t index = SizeClass::Index(size);			 //映射
	_freeList[index].push(ptr);						 //向指定映射的_freeList里面push
	if (_freeList[index].Size() >= _freeList[index].GetMaxSize())
	{
		ListTooLong(_freeList[index], size);		 //太长了就还给CentralCache
	}
}

void ThreadCache::ListTooLong(FreeList& list, size_t size)
{
	void* start = nullptr;
	void* end = nullptr;
	list.rangePop(start, end, list.GetMaxSize());	 //删掉list.GetMaxSize()个
	CentralCache::GetInstance()->ReleaseListToSpans(start, size); //将一点数量的对象释放到span跨度
}