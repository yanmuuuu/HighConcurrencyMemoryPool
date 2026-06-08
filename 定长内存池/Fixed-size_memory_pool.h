#pragma once
#include <iostream>

//定长内存池

//*********************
//何意味:
//例如申请一个int类型的内存池，那么就可以每次拿一份int内存以供使用
//核心实现思路:
//优先从_freeList（空闲链表）取内存→空闲链表空则从预分配的大块内 _memory取→大块内存不足则malloc新的大块（128KB）
//Delete 时先析构对象，再将内存块头插进空闲链表，利用对象内存的前指针大小空间存储下一个空闲节点的地址（复用内存，避免额外开销）
//因为是定长（每次分配 sizeof(T)），所以 _memory 可以直接按 sizeof(T) 向后偏移，无需复杂的内存块分割
//*********************
template<class T>
class ObjectPool
{
public:
	ObjectPool()
		: _memory(nullptr)
		, _freeList(nullptr)
		, _remain_size(0)
	{}

	T* New()
	{
		T* obj = nullptr;
		//在直接问内存池要内存之前，我们可以先检查一下_freeList里面是否有节点
		if (_freeList != nullptr) //有的话直接把_freeList节点拿过来给obj
		{
			void* next = *(void**)_freeList; //取前指针大小的字节
			obj = (T*)_freeList; //将一个节点的内存给obj
			_freeList = next; //_freeList指向下一个节点
		}
		else
		{
			if (_remain_size < sizeof(T)) //说明不够了，该申请新的内存了
			{
				_memory = (char*)malloc(1024 * 128); //申请大量内存
				if (_memory == nullptr) //malloc失败抛异常
				{
					throw std::bad_alloc();
				}
				_remain_size = 1024 * 128;
			}
			//_memory是当前大块内存的可分配起始位置
			//定长分配sizeof(T)后，直接后移指向下次可分配位置
			obj = (T*)_memory;		
			new(obj) T;//使用定位new调用T的构造函数初始化
			size_t size = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
			_memory += size;

			_remain_size -= size; //剩余内存-sizeof(T)
		}
		return obj;
	}

	void Delete(T* obj)
	{
		obj->~T(); //显式调用T的析构函数进行清理

		//采用头插思想
		// _freeList -> ... -> nullptr
		//          ^
		//         obj
		*(void**)obj = _freeList;
		_freeList = obj;
		// 核心操作：*(void**)obj = _freeList;
		// 1. 本质：将_freeList（void*）的地址，写入obj指向内存的「前指针大小字节」（32位4字节/64位8字节）
		// 2. 类型转换逻辑:
		//    - (void**)obj：把obj转为「指向void*的指针」，告诉编译器：obj指向的内存前N字节（N=指针大小）是一个指针地址
		//    - *(void**)obj：解引用后，读写这N字节的指针地址
		// 3. 为什么用void**而非int**/char**:(如果非要使用也可以，毕竟主要是因为二次指针才这样的)
		//    - 语义通用：void*是通用指针类型，贴合“存储任意内存地址”的需求，无需隐式类型转换
		//    - 可读性高：明确表达“操作指针大小的字节”，而非“操作int/char相关的字节”
	}
private:
	char* _memory; //用char* 可以表示一个字节，这样便于计算
	void* _freeList; //头节点
	size_t _remain_size; //当前大块内存(_memory)中剩余的可分配字节数
};