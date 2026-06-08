#pragma once
#include <iostream>
#include <vector>
#include <unordered_map>
#include <map>

#include <algorithm>
#include <time.h>
#include <assert.h>

#include <thread>
#include <mutex>

#ifdef _WIN32
	#include <windows.h>
#else
	//linux...
#endif

static const size_t MAX_BYTES = 262144;          //1024 * 256 -> 256kb //最大字节数
static const size_t MAX_BUCKETS = 208;           //最大_freeList桶数量
static const size_t MAX_PAGE_BUCKETS = 129;      //最大_pageList桶数量(128，但是是直接映射，没有0页这种说法，所以我们直接将128写为129，不要下标0)
static const size_t PAGE_SHIFT = 13;             //定义一页8k，也就是1024 * 8 = 2^13字节

//如果在32位下，假设每一个页大小是8k，也就是2^13，那么最多有2^32 / 2^13 = 2^19，只有524,288个
//size_t完全够用
//但是在64位下，每一个页大小是8k，那么就有2^64 / 2^13 = 5,070,602,400,912,917,605,986,812,821,504个
//size_t不够用，只能使用unsigned long long
#ifdef _WIN64
	using PAGE_ID = unsigned long long;
#elif _WIN32
	using PAGE_ID = size_t;
#endif

// 直接去堆上按页申请空间
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
		void* ptr = VirtualAlloc(0, kpage * (1 << PAGE_SHIFT), MEM_COMMIT | MEM_RESERVE,
			PAGE_READWRITE);
#else
		// linux下brk mmap等
#endif
		if (ptr == nullptr)
			throw std::bad_alloc();
		return ptr;
}

// 直接去释放空间
inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	// sbrk unmmap等
#endif
}

static void*& NextObj(void* obj)
{
	return *(void**)obj;
}

class FreeList //定义一个FreeList类，因为不管是哪个自由链表桶都是一样的结构
{
public:
	FreeList()
		: _freeList(nullptr)
		, _size(0)
		, _maxSize(1)
	{}

	void push(void* obj) //头插
	{
		assert(obj != nullptr);
		NextObj(obj) = _freeList;
		_freeList = obj;
		_size++;
	}

	void rangePush(void* start, void* end, size_t range) //范围头插
	{
		NextObj(end) = _freeList;
		_freeList = start;
		/*
		size_t i = 0;
		while (start)
		{
			i++;
			start = NextObj(start);
		}
		if (i != range)
			int x = 0;
		*/
		_size += range;
	}

	void* pop() //头删
	{
		assert(_freeList != nullptr);
		void* obj = _freeList;
		_freeList = NextObj(_freeList);
		_size--;
		return obj;
	}

	void rangePop(void*& start, void*& end, size_t range) //范围头删
	{
		assert(range <= _size);
		start = _freeList;
		end = start;
		for (size_t i = 0; i < range - 1; i++)
		{
			//条件检测
			/*
			if (end == nullptr)
				int x = 0;*/
			end = NextObj(end);
		}
		_freeList = NextObj(end);
		NextObj(end) = nullptr;
		_size -= range;
	}

	bool empty()
	{
		return _freeList == nullptr;
	}

	size_t& GetMaxSize()
	{
		return _maxSize;
	}

	size_t Size()
	{
		return _size;
	}
private:
	void* _freeList;
	size_t _size;
	size_t _maxSize;
};

class SizeClass //确定哪个桶
{
public:
	// 整体控制在最多10%左右的内碎⽚浪费
	// 字节数                     对齐数           桶的数量
	// [1,128]                  8byte对⻬        freelist[0,16) -> 16个桶
	// [128+1,1024]             16byte对⻬       freelist[16,72) -> 56个桶
	// [1024+1,8*1024]          128byte对⻬      freelist[72,128) -> 56个桶
	// [8*1024+1,64*1024]       1024byte对⻬     freelist[128,184) -> 56个桶
	// [64*1024+1,256*1024]     8*1024byte对⻬   freelist[184,208) -> 24个桶
	// 这样可以让桶的数量只有208个，比起每8个字节为一个桶节省了很多

	//普通写法
	/*
	static inline size_t _RoundUp(size_t bytes, size_t align) //对齐，比如7字节对齐8字节，9字节对齐16字节，129字节对齐144字节
	{
		if (bytes % align == 0)
		{
			return bytes; //无需对齐，因为已经对齐了
		}
		//2 -> 8     (2 / 8 + 1) * 8 -> 8
		//129 -> 144     (129 / 16 + 1) * 16 -> 144
		return (bytes / align + 1) * align;
	}
	*/
	//位运算写法
	static inline size_t _RoundUp(size_t bytes, size_t alignNum)
	{
		return (((bytes) + alignNum - 1) & ~(alignNum - 1));
	}

	static inline size_t RoundUp(size_t bytes) //对齐
	{
		if (bytes <= 128)
		{
			return _RoundUp(bytes, 8);
		}
		else if (bytes <= 1024)
		{
			return _RoundUp(bytes, 16);
		}
		else if (bytes <= 8192)
		{
			return _RoundUp(bytes, 128);
		}
		else if (bytes <= 65536)
		{
			return _RoundUp(bytes, 1024);
		}
		else if (bytes <= 262144)
		{
			return _RoundUp(bytes, 8192);
		}
		else
		{
			return _RoundUp(bytes, 1 << PAGE_SHIFT);
		}
	}

	//普通写法
	/*
	static inline size_t _Index(size_t bytes, size_t alignNum) //计算映射的哪⼀个⾃由链表桶
	{
		if (bytes % align == 0)
			return bytes / align - 1;
		else
			return bytes / align;
	}
	*/
	//位运算写法
	static inline size_t _Index(size_t bytes, size_t align_shift)
	{
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
	}

	static inline size_t Index(size_t bytes) //确定桶
	{
		assert(bytes <= MAX_BYTES);
		if (bytes <= 128)
		{
			//return _Index(bytes, 8);
			return _Index(bytes, 3);
		}
		else if (bytes <= 1024)
		{
			//return _Index(bytes - 128, 16) + 16;
			return _Index(bytes - 128, 4) + 16;
		}
		else if (bytes <= 8192)
		{
			//return _Index(bytes - 1024, 128) + 72; //16 + 56
			return _Index(bytes - 1024, 7) + 72; //16 + 56
		}
		else if (bytes <= 65536)
		{
			//return _Index(bytes - 8192, 1024) + 128; //16 + 56 + 56
			return _Index(bytes - 8192, 10) + 128; //16 + 56 + 56
		}
		else if (bytes <= 262144)
		{
			//return _Index(bytes - 65536, 8192) + 184; //16 + 56 + 56 + 56
			return _Index(bytes - 65536, 13) + 184; //16 + 56 + 56 + 56
		}
		else
		{
			assert(false);
			return -1;
		}
	}

	// ⼀次从中⼼缓存获取多少个对象
	static size_t NumMoveSize(size_t size)
	{
		if (size == 0)
			return 0;
		// [2, 512]，⼀次批量移动多少个对象的(慢启动)上限值
		// ⼩对象⼀次批量上限⾼
		// ⼩对象⼀次批量上限低
		int batchNum = MAX_BYTES / size;
		if (batchNum < 2)
			batchNum = 2;
		if (batchNum > 512)
			batchNum = 512;
		return batchNum;
	}

	//计算一次向PageCache获取几个页
	//单个对象 8byte
	//...
	//单个对象 256k
	static size_t NumMovePage(size_t size)
	{
		size_t num = NumMoveSize(size);
		size_t npage = num * size;
		npage >>= PAGE_SHIFT; //除以8k，也就是8 * 1024字节
		if (npage == 0)
			npage = 1;
		return npage;
	}
};

//管理多个连续页大块内存跨度结构
struct span
{
	PAGE_ID _pageId = 0;                //大块内存起始页的页号(比如一共有100页，这里的66就代表这是第66页的起始位置)
	size_t _n = 0;			            //页的数量(比如3代表这个span有三个页)

	span* _next = nullptr;              //双链表结构，便于插入删除
	span* _prev = nullptr;              

	size_t _objSize = 0;				//切好的小块内存的大小，方便不要size释放
	size_t _useCount = 0;		        //切好小块内存，被分配给thread cache的计数
	void* _freeList = nullptr;          //切好的小块内存的自由链表

	bool _isUse = false;				//是否在被使用
};

class spanList
{
public:
	spanList()
	{
		_head = new span;
		_head->_next = _head;
		_head->_prev = _head;
	}

	void Push_front(span* newspan)
	{
		Insert(Begin(), newspan);
	}

	span* Pop_front()
	{
		span* span = _head->_next;
		Erase(span);
		return span;
	}

	void Insert(span* pos, span* newspan) //插入
	{
		//span pos span -> span newspan pos span
		assert(pos);
		assert(newspan);
		newspan->_next = pos;
		newspan->_prev = pos->_prev;
		pos->_prev->_next = newspan;
		pos->_prev = newspan;
	}

	void Erase(span* pos) //删除
	{
		//span pos span -> span span
		assert(pos);
		assert(pos != _head);
		pos->_next->_prev = pos->_prev;
		pos->_prev->_next = pos->_next;
	}

	span* Begin()
	{
		return _head->_next;
	}

	span* End()
	{
		return _head;
	}

	bool Empty()
	{
		return _head->_next == _head;
	}
private:
	span* _head;
public:
	std::mutex _mutex; //桶锁
};