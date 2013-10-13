#include "prepared_task.hpp"
#include "../runner.hpp"
#include "../../utils/detail/win32_mutex.hpp"
#include "win32_wait_context.hpp"
#include <stdexcept>
#include <windows.h>
using namespace yb;
using namespace yb::detail;

struct prepared_task::impl
{
	detail::win32_mutex m_mutex;
	detail::prepared_task_event_sink * m_runner;
	LONG m_requested_cl;
	LONG m_refcount;
	HANDLE m_done_event;
};

prepared_task::prepared_task()
	: m_pimpl(new impl())
{
	m_pimpl->m_done_event = ::CreateEvent(0, FALSE, FALSE, 0);
	if (!m_pimpl->m_done_event)
		throw std::runtime_error("Failed to create event"); // TODO: win32_error

	m_pimpl->m_runner = 0;
	m_pimpl->m_requested_cl = cl_none;
	m_pimpl->m_refcount = 1;
}

prepared_task::~prepared_task()
{
	::CloseHandle(m_pimpl->m_done_event);
}

void prepared_task::addref()
{
	::InterlockedIncrement(&m_pimpl->m_refcount);
}

void prepared_task::release()
{
	if (::InterlockedDecrement(&m_pimpl->m_refcount) == 0)
		delete this;
}

void prepared_task::request_cancel(cancel_level cl)
{
	cancel_level guess_cl = cl_none;
	while (guess_cl < cl)
	{
		cancel_level previous_cl = ::InterlockedCompareExchange(&m_pimpl->m_requested_cl, cl, guess_cl);
		if (previous_cl == guess_cl)
		{
			// We managed to increase the cancel level, signal the runner
			// to apply it.
			detail::scoped_win32_lock l(m_pimpl->m_mutex);
			if (m_pimpl->m_runner)
				m_pimpl->m_runner->cancel(this);
			return;
		}
		guess_cl = previous_cl;
	}
}

void prepared_task::shadow_prepare_wait(task_wait_preparation_context & prep_ctx)
{
	task_wait_poll_item pi;
	pi.handle = m_pimpl->m_done_event;
	prep_ctx.add_poll_item(pi);
}

void prepared_task::shadow_wait()
{
	::WaitForSingleObject(m_pimpl->m_done_event, INFINITE);
}

void prepared_task::shadow_cancel_and_wait()
{
	detail::scoped_win32_lock l(m_pimpl->m_mutex);
	if (m_pimpl->m_runner)
		m_pimpl->m_runner->cancel_and_wait(this);
}

void prepared_task::attach_event_sink(prepared_task_event_sink & r) throw()
{
	this->addref();

	detail::scoped_win32_lock l(m_pimpl->m_mutex);
	m_pimpl->m_runner = &r;
}

void prepared_task::detach_event_sink() throw()
{
	{
		detail::scoped_win32_lock l(m_pimpl->m_mutex);
		m_pimpl->m_runner = 0;
	}

	this->release();
}

cancel_level prepared_task::requested_cancel_level() const
{
	LONG const volatile & cl = m_pimpl->m_requested_cl;
	return cancel_level(cl);
}

void prepared_task::mark_finished()
{
	::SetEvent(m_pimpl->m_done_event);
}
