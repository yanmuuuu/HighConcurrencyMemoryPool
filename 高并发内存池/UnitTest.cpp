#include "ConcurrentAlloc.h"

struct TreeNode
{
	int _val;
	TreeNode* _left;
	TreeNode* _right;
	TreeNode()
		:_val(0)
		, _left(nullptr)
		, _right(nullptr)
	{
	}
};
void Alloc1()
{
	for (int i = 0; i < 5; i++)
	{
		void* ptr = ConcurrentAlloc(5);
	}
}

void Alloc2()
{
	for (int i = 0; i < 5; i++)
	{
		void* ptr = ConcurrentAlloc(9);
	}
}

void TestThreadCacheAndConcurrentAlloc()
{
	std::thread thread1(Alloc1);	
	std::thread thread2(Alloc2);
	thread1.join();
	thread2.join();	
}

void TestConcurrentAlloc1()
{
	void* p1 = ConcurrentAlloc(6);
	void* p2 = ConcurrentAlloc(1024 * 257);
	void* p3 = ConcurrentAlloc(1024 * 8 * 129);
	void* p4 = ConcurrentAlloc(7);
	void* p5 = ConcurrentAlloc(8);
	std::cout << p1 << std::endl;
	std::cout << p2 << std::endl;
	std::cout << p3 << std::endl;
	std::cout << p4 << std::endl;
	std::cout << p5 << std::endl;
	ConcurrentFree(p1);
	ConcurrentFree(p2);
	ConcurrentFree(p3);
	ConcurrentFree(p4);
	ConcurrentFree(p5);
}

void TestConcurrentAlloc2()
{
	for (size_t i = 0; i < 1024; i++)
	{
		void* p1 = ConcurrentAlloc(6);
		std::cout << p1 << std::endl;
	}
	void* p2 = ConcurrentAlloc(1);
	std::cout << p2 << std::endl;
}

void TestConcurrentAlloc3()
{
	std::vector<void*> v;
	for (size_t i = 0; i < 1024; i++)
	{
		v.push_back(ConcurrentAlloc(6));
	}
	for (auto& e : v)
	{
		ConcurrentFree(e);
	}
}

//»ù´¡²âÊÔ

/*
int main()
{
	//TestThreadCacheAndConcurrentAlloc();
	//TestConcurrentAlloc1();
	//TestConcurrentAlloc2();
	TestConcurrentAlloc3();
	return 0;
}*/