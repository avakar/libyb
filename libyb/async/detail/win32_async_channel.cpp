#include "../async_channel.hpp"
#include "win32_handle_task.hpp"
#include "../../utils/detail/win32_mutex.hpp"
#include <stdexcept>

using namespace yb;
using namespace yb::detail;

struct async_channel_base::impl
{
	impl()
	{
		hDataReady = CreateEvent(0, TRUE, FALSE, 0);
		if (!hDataReady)
			throw std::runtime_error("couldn't create event");
	}

	~impl()
	{
		CloseHandle(hDataReady);
	}

	win32_mutex cs;
	HANDLE hDataReady;
};

async_channel_base::scoped_lock::scoped_lock(async_channel_base * ch)
	: m_ch(ch)
{
	m_ch->m_pimpl->cs.lock();
}

async_channel_base::scoped_lock::~scoped_lock()
{
	m_ch->m_pimpl->cs.unlock();
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
	return make_win32_handle_task(m_pimpl->hDataReady, [](cancel_level cl) { return cl < cl_abort; });
}

void async_channel_base::set()
{
	SetEvent(m_pimpl->hDataReady);
}

void async_channel_base::reset()
{
	ResetEvent(m_pimpl->hDataReady);
}

bool async_channel_base::empty() const
{
	return WaitForSingleObject(m_pimpl->hDataReady, 0) == WAIT_TIMEOUT;
}
