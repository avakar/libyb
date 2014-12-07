#include "../sync_runner.hpp"
#include "../../utils/detail/win32_mutex.hpp"
#include "win32_runner_registry.hpp"
#include "win32_wait_context.hpp"
#include <stdexcept>
#include <list>
#include <cassert>
#include <windows.h>
using namespace yb;

struct sync_runner::impl
	: public runner_registry
{
	detail::win32_mutex m_mutex;
	HANDLE m_update_event;
	runner_work * m_update_chain;

	DWORD m_associated_thread;

	std::vector<HANDLE> m_handles;
	std::vector<handle_completion_sink *> m_handle_sinks;

	void add_handle(HANDLE h, handle_completion_sink & sink)
	{
		// XXX
		m_handles.push_back(h);
		m_handle_sinks.push_back(&sink);
	}

	void remove_handle(HANDLE h, handle_completion_sink & sink)
	{
		for (size_t i = 0; i < m_handle_sinks.size(); ++i)
		{
			if (m_handles[i+1] == h && m_handle_sinks[i] == &sink)
			{
				std::swap(m_handles[i+1], m_handles.back());
				std::swap(m_handle_sinks[i], m_handle_sinks.back());

				m_handles.pop_back();
				m_handle_sinks.pop_back();
				break;
			}
		}
	}

	void schedule_work(runner_work & w) throw() override
	{
		w.prepend_to_chain(m_update_chain);
		::SetEvent(m_update_event);
	}
};

sync_runner::sync_runner(bool associate_thread_now)
	: m_pimpl(new impl())
{
	m_pimpl->m_handles.reserve(64);
	m_pimpl->m_update_event = CreateEvent(0, FALSE, FALSE, 0);
	if (!m_pimpl->m_update_event)
		throw std::runtime_error("Failed to create an event");

	m_pimpl->m_handles.push_back(m_pimpl->m_update_event);

	m_pimpl->m_associated_thread = associate_thread_now? ::GetCurrentThreadId(): 0;
	m_pimpl->m_update_chain = 0;
}

sync_runner::~sync_runner()
{
	CloseHandle(m_pimpl->m_update_event);
}

/*void sync_runner::schedule_work(runner_work & work) throw()
{
	m_pimpl->schedule_work(work);
}*/

void sync_runner::associate_current_thread()
{
	assert(m_pimpl->m_associated_thread == 0);
	m_pimpl->m_associated_thread = ::GetCurrentThreadId();
}

void sync_runner::submit(detail::prepared_task_base * pt)
{
}

void sync_runner::run_until(detail::prepared_task_base * focused_pt)
{
	assert(m_pimpl->m_associated_thread == GetCurrentThreadId());

	if (focused_pt)
		focused_pt->addref();

	while (!focused_pt || !focused_pt->completed())
	{
		{
			detail::scoped_win32_lock l(m_pimpl->m_mutex);

			runner_work * next = m_pimpl->m_update_chain;
			m_pimpl->m_update_chain = 0;

			for (runner_work * cur = next; cur; cur = next)
				next = cur->work(*m_pimpl);
		}

		DWORD res = ::WaitForMultipleObjects(m_pimpl->m_handles.size(), m_pimpl->m_handles.data(), FALSE, INFINITE);
		if (res == WAIT_OBJECT_0)
			continue;

		m_pimpl->m_handle_sinks[res - WAIT_OBJECT_0 - 1]->on_signaled(*m_pimpl, m_pimpl->m_handles[res - WAIT_OBJECT_0]);
	}

	if (focused_pt)
		focused_pt->release();
}
