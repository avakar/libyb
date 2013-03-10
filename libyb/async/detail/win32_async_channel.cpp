#include "../async_channel.hpp"
#include "win32_handle_task.hpp"
using namespace yb;
using namespace yb::detail;

struct async_channel_base::impl
{
	impl()
	{
		hDataReady = CreateEvent(0, TRUE, FALSE, 0);
		if (!hDataReady)
			throw std::runtime_error("couldn't create event");
		InitializeCriticalSection(&cs);
	}

	~impl()
	{
		DeleteCriticalSection(&cs);
		CloseHandle(hDataReady);
	}

	CRITICAL_SECTION cs;
	HANDLE hDataReady;
};

async_channel_base::scoped_lock::scoped_lock(async_channel_base * ch)
	: m_ch(ch)
{
	EnterCriticalSection(&m_ch->m_pimpl->cs);
}

async_channel_base::scoped_lock::~scoped_lock()
{
	LeaveCriticalSection(&m_ch->m_pimpl->cs);
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
