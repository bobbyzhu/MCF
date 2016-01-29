#include <MCF/StdMCF.hpp>
#include <MCF/Containers/FlatMap.hpp>
#include <MCF/Containers/Vector.hpp>
#include <MCF/Containers/StaticVector.hpp>
#include <MCF/Containers/List.hpp>
#include <MCF/Containers/RingQueue.hpp>
#include <cstdio>

using namespace MCF;

struct foo {
	int a;

	explicit foo(int r)
		: a(r)
	{
		std::printf("foo(): a = %d\n", a);
	}
	foo(const foo &r)
		: a(r.a)
	{
		std::printf("foo(const foo &): a = %d\n", a);
	}
	foo(foo &&r) noexcept
		: a(std::exchange(r.a, -1))
	{
		std::printf("foo(foo &&): a = %d\n", a);
	}
	~foo(){
		if(a == -1){
			return;
		}
		std::printf("~foo(): a = %d\n", a);
	}
};

bool operator<(const foo &l, const foo &r) noexcept {
	std::printf("cmp(foo, foo) ");
	return l.a < r.a;
}
bool operator<(int l, const foo &r) noexcept {
	std::printf("cmp(int, foo) ");
	return l < r.a;
}
bool operator<(const foo &l, int r) noexcept {
	std::printf("cmp(foo, int) ");
	return l.a < r;
}

// template class FlatMap<foo, int>;

template class Vector<foo>;
template class StaticVector<foo, 100>;
template class List<foo>;
template class RingQueue<foo>;

extern "C" unsigned MCFCRT_Main(){
	StaticVector<foo, 100> q;
	for(int i = 0; i < 10; ++i){
		q.Push(i);
	}
	for(int i = 0; i < 10; ++i){
		q.Pop();
	}
	for(int i = 0; i < 10; ++i){
		q.Push(i);
	}
	return 0;
}
