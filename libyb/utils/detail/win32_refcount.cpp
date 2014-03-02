#include "../refcount.hpp"
#include <windows.h>

yb::refcount::refcount(size_t initial)
	: m_value(initial)
{
}

size_t yb::refcount::addref()
{
	return InterlockedIncrement(&m_value);
}

size_t yb::refcount::release()
{
	return InterlockedDecrement(&m_value);
}
