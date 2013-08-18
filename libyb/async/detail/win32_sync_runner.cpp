#include "../sync_runner.hpp"
#include "../../utils/detail/win32_mutex.hpp"
#include "win32_wait_context.hpp"
#include <stdexcept>
#include <list>
#include <cassert>
#include <windows.h>
using namespace yb;

struct sync_runner::impl
{
	struct task_entry
	{
		task_wait_memento memento;
		detail::prepared_task * pt;
	};

	detail::win32_mutex m_mutex;
	HANDLE m_update_event;
	DWORD m_associated_thread;
	std::list<task_entry> m_tasks;
};

sync_runner::sync_runner(bool associate_thread_now)
	: m_pimpl(new impl())
{
	m_pimpl->m_update_event = CreateEvent(0, FALSE, FALSE, 0);
	if (!m_pimpl->m_update_event)
		throw std::runtime_error("Failed to create an event");
	m_pimpl->m_associated_thread = associate_thread_now? ::GetCurrentThreadId(): 0;
}

sync_runner::~sync_runner()
{
	for (std::list<impl::task_entry>::iterator it = m_pimpl->m_tasks.begin(), eit = m_pimpl->m_tasks.end(); it != eit; ++it)
	{
		it->pt->cancel_and_wait();
		it->pt->detach_event_sink();
	}

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

	task_wait_preparation_context prep_ctx;
	task_wait_preparation_context_impl * prep_ctx_impl = prep_ctx.get();

	task_wait_finalization_context fin_ctx;
	fin_ctx.prep_ctx = &prep_ctx;

	detail::scoped_win32_lock l(m_pimpl->m_mutex);

	bool done = false;
	while (!done && !m_pimpl->m_tasks.empty())
	{
		for (impl::task_entry & pe: m_pimpl->m_tasks)
			pe.pt->apply_cancel();

		prep_ctx.clear();
		prep_ctx_impl->m_handles.push_back(m_pimpl->m_update_event);

		for (impl::task_entry & pe: m_pimpl->m_tasks)
		{
			task_wait_memento_builder mb(prep_ctx);
			pe.pt->prepare_wait(prep_ctx);
			pe.memento = mb.finish();
		}

		if (prep_ctx_impl->m_finished_tasks)
		{
			fin_ctx.selected_poll_item = 0;
			for (std::list<impl::task_entry>::iterator it = m_pimpl->m_tasks.begin(), eit = m_pimpl->m_tasks.end(); it != eit;)
			{
				impl::task_entry & pe = *it;

				if (pe.memento.finished_task_count)
				{
					fin_ctx.finished_tasks = pe.memento.finished_task_count;
					if (pe.pt->finish_wait(fin_ctx))
					{
						if (pe.pt == focused_pt)
							done = true;
						pe.pt->detach_event_sink();
						it = m_pimpl->m_tasks.erase(it);
					}
					else
					{
						++it;
					}
				}
				else
				{
					++it;
				}
			}
		}
		else
		{
			::ResetEvent(m_pimpl->m_update_event);
			m_pimpl->m_mutex.unlock();
			DWORD res = WaitForMultipleObjects(prep_ctx_impl->m_handles.size(), prep_ctx_impl->m_handles.data(), FALSE, INFINITE);
			m_pimpl->m_mutex.lock();

			if (res != WAIT_OBJECT_0)
			{
				size_t selected = res - WAIT_OBJECT_0;
				fin_ctx.finished_tasks = 0;
				for (std::list<impl::task_entry>::iterator it = m_pimpl->m_tasks.begin(), eit = m_pimpl->m_tasks.end(); it != eit; ++it)
				{
					impl::task_entry & pe = *it;
					if (pe.memento.poll_item_first <= selected && pe.memento.poll_item_last > selected)
					{
						fin_ctx.selected_poll_item = selected - pe.memento.poll_item_first;
						if (pe.pt->finish_wait(fin_ctx))
						{
							if (pe.pt == focused_pt)
								done = true;
							pe.pt->detach_event_sink();
							m_pimpl->m_tasks.erase(it);
						}
						break;
					}
				}
			}
		}
	}
}

void sync_runner::submit(detail::prepared_task * pt)
{
	impl::task_entry te;
	te.pt = pt;

	detail::scoped_win32_lock l(m_pimpl->m_mutex);
	m_pimpl->m_tasks.push_back(te);
	pt->attach_event_sink(*this);
	::SetEvent(m_pimpl->m_update_event);
}

void sync_runner::cancel(detail::prepared_task *, cancel_level) throw()
{
	detail::scoped_win32_lock l(m_pimpl->m_mutex);
	::SetEvent(m_pimpl->m_update_event);
}

void sync_runner::cancel_and_wait(detail::prepared_task * pt) throw()
{
	assert(m_pimpl->m_associated_thread != 0);

	if (m_pimpl->m_associated_thread == ::GetCurrentThreadId())
	{
		for (std::list<impl::task_entry>::iterator it = m_pimpl->m_tasks.begin(), eit = m_pimpl->m_tasks.end(); it != eit; ++it)
		{
			if (it->pt == pt)
			{
				m_pimpl->m_tasks.erase(it);
				break;
			}
		}

		pt->cancel_and_wait();
		pt->detach_event_sink(); 
	}
	else
	{
		pt->request_cancel(cl_kill);
		pt->shadow_wait();
	}
}
