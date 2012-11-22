#include "../timer.hpp"
using namespace yb;

struct timer::impl
{
};

timer::timer()
	: m_pimpl(new impl())
{
}

timer::~timer()
{
}

task<void> timer::wait_ms(int milliseconds)
{
	// XXX
	return async::value();
}
