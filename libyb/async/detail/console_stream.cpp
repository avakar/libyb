#include "../console_stream.hpp"
using namespace yb;

console_stream::console_stream(console_stream && o)
	: m_pimpl(o.m_pimpl)
{
	o.m_pimpl = 0;
}

console_stream & console_stream::operator=(console_stream o)
{
	swap(*this, o);
	return *this;
}

void yb::swap(console_stream & lhs, console_stream & rhs)
{
	std::swap(lhs.m_pimpl, rhs.m_pimpl);
}
