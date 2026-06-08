#pragma once
#include "Common.h"

//单例模式(饿汉模式)
class CentralCache
{
public:
	//获取实例
	static CentralCache* GetInstance()
	{
		return &_sInst;
	}

	// 获取⼀个⾮空的span
	span* GetOneSpan(spanList& list, size_t size);

	// 从中⼼缓存获取⼀定数量的对象给thread cache
	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size);

	//将一点数量的对象释放到span跨度
	void ReleaseListToSpans(void* start, size_t byte_size);
private:
	spanList _spanList[MAX_BUCKETS]; //和ThreadCheche逻辑是一样的，对应字节找对应桶
private:
	CentralCache()
	{ }
	CentralCache(const CentralCache&) = delete;
	static CentralCache _sInst;
};