#include "../refcount.hpp"
using namespace yb;

refcount::refcount(size_t initial)
	: m_value(initial)
{
}

size_t refcount::addref()
{
	return m_value.fetch_add(1);
}

size_t refcount::release()
{
	return m_value.fetch_sub(1);
}
