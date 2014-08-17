#include "../async_runner.hpp"
#include "../sync_runner.hpp"
#include "../channel.hpp"
#include <stdexcept>
#include <windows.h>
using namespace yb;

struct async_runner::impl
{
	sync_runner m_runner;
	channel<void> m_stop_promise;
	HANDLE m_thread_handle;

	impl()
		: m_runner(/*associate_thread_now=*/false), m_stop_promise(channel<void>::create_finite())
	{
	}

	static DWORD CALLBACK thread_proc(LPVOID param);
};

DWORD CALLBACK async_runner::impl::thread_proc(LPVOID param)
{
	impl * pimpl = static_cast<impl *>(param);
	pimpl->m_runner.associate_current_thread();
	pimpl->m_runner.run(pimpl->m_stop_promise.receive());
	return 0;
}

async_runner::async_runner()
	: m_pimpl(new impl())
{
	DWORD thread_id;
	m_pimpl->m_thread_handle = ::CreateThread(0, 0, &impl::thread_proc, m_pimpl.get(), 0, &thread_id);
	if (!m_pimpl->m_thread_handle)
		throw std::runtime_error("Failed to create thread"); // TODO: win32_error
}

async_runner::~async_runner()
{
	m_pimpl->m_stop_promise.send_sync();
	::WaitForSingleObject(m_pimpl->m_thread_handle, INFINITE);
	::CloseHandle(m_pimpl->m_thread_handle);
}

void async_runner::submit(detail::prepared_task * pt)
{
	m_pimpl->m_runner.submit(pt);
}

void async_runner::run_until(detail::prepared_task * pt)
{
	pt->shadow_wait();
}
