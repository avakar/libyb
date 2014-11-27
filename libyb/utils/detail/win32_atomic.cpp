#include "../atomic.hpp"
#include <windows.h>

yb::atomic_uint::atomic_uint(value_type initial)
	: m_value(initial)
{
}

#ifndef _MSC_VER

size_t yb::atomic_uint::load()
{
	return m_value.load();
}

size_t yb::atomic_uint::fetch_add(value_type v)
{
	return m_value.fetch_add(v);
}

size_t yb::atomic_uint::fetch_sub(value_type v)
{
	return m_value.fetch_sub(v);
}

bool yb::atomic_uint::compare_exchange_weak(value_type & expected, value_type desired)
{
	return m_value.compare_exchange_weak(expected, desired);
}

bool yb::atomic_uint::compare_exchange_strong(value_type & expected, value_type desired)
{
	return m_value.compare_exchange_strong(expected, desired);
}

#else

size_t yb::atomic_uint::load()
{
	size_t const volatile & v = m_value;
	return v;
}

size_t yb::atomic_uint::fetch_add(value_type v)
{
	if (v == 1)
		return InterlockedIncrement(&m_value) - 1;
	else
		return InterlockedAdd((LONG *)&m_value, v) - v;
}

size_t yb::atomic_uint::fetch_sub(value_type v)
{
	if (v == 1)
		return InterlockedDecrement(&m_value) + 1;
	else
		return InterlockedAdd((LONG *)&m_value, -(LONG)v) + v;
}

bool yb::atomic_uint::compare_exchange_weak(value_type & expected, value_type desired)
{
	value_type new_value = InterlockedCompareExchange(&m_value, desired, expected);
	if (new_value != expected)
	{
		expected = new_value;
		return false;
	}

	return true;
}

bool yb::atomic_uint::compare_exchange_strong(value_type & expected, value_type desired)
{
	return this->compare_exchange_weak(expected, desired);
}

#endif
