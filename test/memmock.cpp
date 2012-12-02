#include <cstdlib>
#include <exception>
#include <new>
#include "memmock.h"

#ifdef _MSC_VER
#define thread_local __declspec(thread)
#endif

#ifdef __GNUC__
#define thread_local __thread
#endif

static thread_local size_t g_alloc_count = 0;
static thread_local alloc_filter_registration g_reg = {};

size_t get_total_alloc_count()
{
	return g_alloc_count;
}

alloc_filter_registration set_alloc_filter(alloc_filter_registration reg)
{
	alloc_filter_registration res = g_reg;
	g_reg = reg;
	return res;
}

alloc_filter_registration set_alloc_filter(bool (*filter)(size_t alloc_index, void * context), void * context)
{
	alloc_filter_registration reg = { filter, context };
	alloc_filter_registration res = g_reg;
	g_reg = reg;
	return res;
}

void * operator new(size_t size)
{
	++g_alloc_count;
	if (!g_reg.filter || g_reg.filter(g_alloc_count, g_reg.context))
		return malloc(size);
	else
		throw std::bad_alloc();
}

void operator delete(void * ptr)
{
	free(ptr);
}

void * operator new[](size_t size)
{
	++g_alloc_count;
	if (!g_reg.filter || g_reg.filter(g_alloc_count, g_reg.context))
		return malloc(size);
	else
		throw std::bad_alloc();
}

void operator delete[](void * ptr)
{
	free(ptr);
}

void * operator new(size_t size, std::nothrow_t) throw()
{
	++g_alloc_count;
	if (!g_reg.filter || g_reg.filter(g_alloc_count, g_reg.context))
		return malloc(size);
	else
		return 0;
}

void operator delete(void * ptr, std::nothrow_t)
{
	free(ptr);
}

void * operator new[](size_t size, std::nothrow_t) throw()
{
	++g_alloc_count;
	if (!g_reg.filter || g_reg.filter(g_alloc_count, g_reg.context))
		return malloc(size);
	else
		return 0;
}

void operator delete[](void * ptr, std::nothrow_t)
{
	free(ptr);
}
