#include "../sync_runner.hpp"
#include "../../utils/detail/win32_mutex.hpp"
#include "win32_wait_context.hpp"
#include <stdexcept>
#include <list>
#include <cassert>
#include <windows.h>
using namespace yb;

struct task_ctx
{
	prepared_task * task;
	task_ctx * update_next;
	cancel_level cl;
	cancel_level next_cl;
	
	enum state_t { st_idle, st_started, st_finished };
	state_t state;
};

struct sync_runner::impl
	: public runner_registry
	, public prepared_task_event_sink
{
	detail::win32_mutex m_mutex;
	HANDLE m_update_event;
	prepared_task * m_update_chain;

	DWORD m_associated_thread;

	std::vector<HANDLE> m_handles;
	std::vector<handle_completion_sink *> m_handle_sinks;

	void post(runner_posted_routine & sink) throw()
	{
		// XXX
	}

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

	prepared_task * schedule_update(prepared_task * pt) throw() override
	{
		detail::scoped_win32_lock l(m_mutex);
		prepared_task * res = m_update_chain;
		m_update_chain = pt;
		::SetEvent(m_update_event);
		return res;
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

void sync_runner::associate_current_thread()
{
	assert(m_pimpl->m_associated_thread == 0);
	m_pimpl->m_associated_thread = ::GetCurrentThreadId();
}

void sync_runner::run_until(detail::prepared_task * focused_pt)
{
	assert(m_pimpl->m_associated_thread == GetCurrentThreadId());

	for (;;)
	{
		{
			detail::scoped_win32_lock l(m_pimpl->m_mutex);

			prepared_task * next = m_pimpl->m_update_chain;
			m_pimpl->m_update_chain = 0;

			bool done = false;
			for (task_ctx * cur = next; cur; cur = next)
			{
				next = cur->update_next;
				cur->update_next = 0;

				if (cur->next_cl > cur->cl)
				{
					cur->task->cancel(m_pimpl, cur->next_cl);
					cur->cl = cur->next_cl;
				}

				if (cur->state == task_ctx::st_idle)
				{
					cur->task->start(*m_pimpl, *cur);
					cur->started = true;
				}

				if (cur->state == task_ctx::st_finished)
				{
					cur->task->release();
					if (cur == focused_pt)
						done = true;
					delete cur;
				}
			}

			if (done)
				break;
		}

		DWORD res = ::WaitForMultipleObjects(m_pimpl->m_handles.size(), m_pimpl->m_handles.data(), FALSE, INFINITE);
		if (res == WAIT_OBJECT_0)
			continue;

		m_pimpl->m_handle_sinks[res - WAIT_OBJECT_0 - 1]->on_signaled(m_pimpl, m_pimpl->m_handles[res - WAIT_OBJECT_0];
	}
}

void sync_runner::submit(detail::prepared_task * pt)
{
	this->schedule_update(pt);
}

void sync_runner::schedule_update(detail::prepared_task * pt) throw()
{
	detail::scoped_win32_lock l(m_pimpl->m_mutex);
	pt->join_update_chain(m_pimpl->m_update_chain);
	m_pimpl->m_update_chain = pt;
	::SetEvent(m_pimpl->m_update_event);
}
