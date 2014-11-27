#include "../refcount.hpp"
#include <windows.h>

yb::refcount::refcount(size_t initial)
	: m_value(initial)
{
}

size_t yb::refcount::addref()
{
	return m_value.fetch_add(1) + 1;
}

size_t yb::refcount::release()
{
	return m_value.fetch_sub(1) - 1;
}
