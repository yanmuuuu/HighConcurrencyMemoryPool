#include "PageCache.h"

PageCache PageCache::_sInst;

//在PageList里面找有没有符合要求的_freeList的节点，没有就向系统申请一个大块内存
span* PageCache::NewSpan(size_t size) //这里的size是页数
{
	assert(size);
	if (size > MAX_PAGE_BUCKETS - 1) //大于128页就直接申请
	{
		void* ptr = SystemAlloc(size);
		span* newspan = spanPool.New();
		newspan->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
		newspan->_n = size;
		_idSpanMap[newspan->_pageId] = newspan;
		return newspan;
	}
	//先直接看size桶有没有
	if (!_spanlist[size].Empty())
	{
		span* kspan = _spanlist[size].Pop_front();
		for (size_t i = 0; i < kspan->_n; i++)
		{
			_idSpanMap[kspan->_pageId + i] = kspan;
		}
		return kspan;
	}
	//size桶没有就往后找有的
	//如果找到直接切下来size大小返回，i - size插入i - size桶
	for (int i = size + 1; i < MAX_PAGE_BUCKETS; i++)
	{
		if (!_spanlist[i].Empty())
		{
			span* nspan = _spanlist[i].Pop_front();
			//span* kspan = new span; //因为我们tcmalloc是要代替malloc和new的，要是调用new不久相当于左脚踩右脚吗
			span* kspan = spanPool.New();
			//找页数大小为三的，发现100号桶有内存
			//那么切除为3块 与 100 - 3块
			//此时id 变为100与100 + 3，因为少了三块，往后移3块
			kspan->_n = size;
			kspan->_pageId = nspan->_pageId;

			nspan->_n -= size;
			nspan->_pageId += size;

			_spanlist[nspan->_n].Push_front(nspan);

			//存储nspan的首尾地址就可以(相当于挂接第103起始位置和对应的nspan)
			_idSpanMap[nspan->_pageId] = nspan;
			_idSpanMap[nspan->_pageId + nspan->_n - 1] = nspan;
			//建立id和span的映射，方便CentralCache回收小块内存时，查找对应的span
			for (size_t i = 0; i < kspan->_n; i++)
			{
				_idSpanMap[kspan->_pageId + i] = kspan;
			}

			return kspan;
		}
	}
	//如果没有符合的，那么就向堆(系统)申请一个大块内存(128 * 8k)
	span* bigspan = spanPool.New(); //new只是创建一个span结构体对象(用来管理页的元数据，存_pageId、_n、_freeList等)
	void* ptr = SystemAlloc(MAX_PAGE_BUCKETS - 1); //申请内存
	bigspan->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
	bigspan->_n = MAX_PAGE_BUCKETS - 1;
	_spanlist[bigspan->_n].Push_front(bigspan);
	//为什么选择递归掉自己而不是直接切分返回呢
	//1.现代计算机太快了，再遍历128次的时间损耗可以忽略不计
	//2.在这个微乎其微的时间损耗下，我们更偏向于进行代码复用
	return NewSpan(size);
}

span* PageCache::MapObjectToSpan(void* obj)
{
	PAGE_ID id = (PAGE_ID)obj >> PAGE_SHIFT;

	std::unique_lock<std::mutex> lock(_pageMutex); //c++新特性，出了这个函数就自动解锁

	auto ret = _idSpanMap.find(id);
	if (ret != _idSpanMap.end())
	{
		return ret->second;
	}
	else
	{
		assert(false);
		return nullptr;
	}
}

void PageCache::ReleaseSpanToPageCache(span* obj)
{
	//大于128页的直接还给堆
	if (obj->_n > MAX_PAGE_BUCKETS - 1)
	{
		void* ptr = (void*)(obj->_pageId << PAGE_SHIFT);
		SystemFree(ptr);
		spanPool.Delete(obj);;
		return;
	}
	//向前合并
	while (1)
	{
		PAGE_ID prevId = obj->_pageId - 1;
		auto ret = _idSpanMap.find(prevId);
		if (ret == _idSpanMap.end())
		{
			break;
		}
		span* prevSpan = ret->second;
		if (prevSpan->_isUse == true)
		{
			break;
		}
		if (prevSpan->_n + obj->_n > MAX_PAGE_BUCKETS - 1)
		{
			break;
		}
		obj->_pageId = prevSpan->_pageId;
		obj->_n += prevSpan->_n;
		_spanlist[prevSpan->_n].Erase(prevSpan);
		//delete prevSpan; 同理
		spanPool.Delete(prevSpan);
	}
	//向后合并
	while (1)
	{
		PAGE_ID nextId = obj->_pageId + obj->_n;
		auto ret = _idSpanMap.find(nextId);
		if (ret == _idSpanMap.end())
		{
			break;
		}
		span* nextSpan = ret->second;
		if (nextSpan->_isUse == true)
		{
			break;
		}
		if (nextSpan->_n + obj->_n > MAX_PAGE_BUCKETS - 1)
		{
			break;
		}
		obj->_n += nextSpan->_n;
		_spanlist[nextSpan->_n].Erase(nextSpan);
		spanPool.Delete(nextSpan);
	}
	_spanlist[obj->_n].Push_front(obj);
	obj->_isUse = false;
	_idSpanMap[obj->_pageId] = obj;
	_idSpanMap[obj->_pageId + obj->_n - 1] = obj;
}