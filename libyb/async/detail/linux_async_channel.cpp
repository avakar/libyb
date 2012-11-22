#include "../async_channel.hpp"
using namespace yb;
using namespace yb::detail;

struct async_channel_base::impl
{
};

async_channel_base::scoped_lock::scoped_lock(async_channel_base * ch)
	: m_ch(ch)
{
}

async_channel_base::scoped_lock::~scoped_lock()
{
}

async_channel_base::async_channel_base()
	: m_pimpl(new impl())
{
}

async_channel_base::~async_channel_base()
{
}

task<void> async_channel_base::wait()
{
	// XXX
	return async::value();
}

void async_channel_base::set()
{
	// XXX
}

void async_channel_base::reset()
{
	// XXX
}
