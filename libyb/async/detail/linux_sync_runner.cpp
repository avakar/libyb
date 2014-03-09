#include "../sync_runner.hpp"
#include "../../utils/detail/pthread_mutex.hpp"
#include "../../utils/detail/linux_event.hpp"
#include "linux_wait_context.hpp"
#include <sys/eventfd.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdexcept>
#include <list>
#include <cassert>
using namespace yb;

struct sync_runner::impl
{
    struct task_entry
    {
        task_wait_memento memento;
        detail::prepared_task * pt;
    };

    detail::pthread_mutex m_mutex;
	detail::linux_event m_update_event;
    pid_t m_associated_thread;
    std::list<task_entry> m_tasks;
};

sync_runner::sync_runner(bool associate_thread_now)
    : m_pimpl(new impl())
{
    m_pimpl->m_associated_thread = associate_thread_now? syscall(SYS_gettid): 0;
}

sync_runner::~sync_runner()
{
    for (std::list<impl::task_entry>::iterator it = m_pimpl->m_tasks.begin(), eit = m_pimpl->m_tasks.end(); it != eit; ++it)
    {
        it->pt->cancel_and_wait();
        it->pt->detach_event_sink();
    }
}

void sync_runner::associate_current_thread()
{
    assert(m_pimpl->m_associated_thread == 0);
    m_pimpl->m_associated_thread = syscall(SYS_gettid);
}

void sync_runner::run_until(detail::prepared_task * focused_pt)
{
    assert(m_pimpl->m_associated_thread == syscall(SYS_gettid));

    task_wait_preparation_context prep_ctx;
    task_wait_preparation_context_impl * prep_ctx_impl = prep_ctx.get();

    task_wait_finalization_context fin_ctx;
    fin_ctx.prep_ctx = &prep_ctx;

    detail::scoped_pthread_lock l(m_pimpl->m_mutex);

    bool done = false;
    while (!done && !m_pimpl->m_tasks.empty())
    {
        prep_ctx.clear();

		prep_ctx_impl->m_pollfds.push_back(m_pimpl->m_update_event.get_poll());

        for (auto it = m_pimpl->m_tasks.begin(); it != m_pimpl->m_tasks.end(); ++it)
        {
            impl::task_entry & pe = *it;
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
			m_pimpl->m_update_event.reset();
            m_pimpl->m_mutex.unlock();
            int r = poll(prep_ctx_impl->m_pollfds.data(), prep_ctx_impl->m_pollfds.size(), -1);
            if (r == -1)
                throw std::runtime_error("poll failed"); // XXX
            m_pimpl->m_mutex.lock();

            assert(r != 0);
            for (size_t i = 0; r; ++i)
            {
                struct pollfd & p = prep_ctx_impl->m_pollfds[i];
                if (!p.revents)
                    continue;
                --r;

                if (i == 0)
                    continue;

                fin_ctx.finished_tasks = 0;
                for (std::list<impl::task_entry>::iterator it = m_pimpl->m_tasks.begin(), eit = m_pimpl->m_tasks.end(); it != eit; ++it)
                {
                    impl::task_entry & pe = *it;
                    if (pe.memento.poll_item_first <= i && pe.memento.poll_item_last > i)
                    {
                        fin_ctx.selected_poll_item = i;
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

    detail::scoped_pthread_lock l(m_pimpl->m_mutex);
    m_pimpl->m_tasks.push_back(te);
    pt->attach_event_sink(*this);

	m_pimpl->m_update_event.set();
}

void sync_runner::cancel(detail::prepared_task *) throw()
{
	m_pimpl->m_update_event.set();
}

void sync_runner::cancel_and_wait(detail::prepared_task * pt) throw()
{
    assert(m_pimpl->m_associated_thread != 0);

    if (m_pimpl->m_associated_thread == syscall(SYS_gettid))
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
