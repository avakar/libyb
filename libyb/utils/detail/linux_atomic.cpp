#include "../atomic.hpp"

yb::atomic_uint::atomic_uint(value_type initial)
	: m_value(initial)
{
}

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
