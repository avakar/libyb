#include "prepared_task.hpp"
#include "../runner.hpp"
#include "linux_wait_context.hpp"
#include "../../utils/detail/linux_event.hpp"
#include "../../utils/detail/pthread_mutex.hpp"
#include <atomic>
#include <stdexcept>
using namespace yb;
using namespace yb::detail;

struct prepared_task::impl
{
	detail::pthread_mutex m_mutex;
	detail::prepared_task_event_sink * m_runner;
	std::atomic<int> m_cl;
	std::atomic<int> m_refcount;
	yb::detail::linux_event m_done_event;
};

prepared_task::prepared_task()
	: m_pimpl(new impl())
{
	m_pimpl->m_runner = 0;
	m_pimpl->m_cl = cl_none;
	m_pimpl->m_refcount = 1;
}

prepared_task::~prepared_task()
{
}

void prepared_task::addref() throw()
{
	++m_pimpl->m_refcount;
}

void prepared_task::release() throw()
{
	if (--m_pimpl->m_refcount == 0)
		delete this;
}

void prepared_task::request_cancel(cancel_level cl) throw()
{
	cancel_level guess_cl = cl_none;
	while (guess_cl < cl)
	{
		if (std::atomic_compare_exchange_weak(&m_pimpl->m_cl, &guess_cl, cl))
		{
			// We managed to increase the cancel level, signal the runner
			// to apply it.
			detail::scoped_pthread_lock l(m_pimpl->m_mutex);
			if (m_pimpl->m_runner)
				m_pimpl->m_runner->cancel(this);
			return;
		}
	}
}

void prepared_task::shadow_prepare_wait(task_wait_preparation_context & prep_ctx, cancel_level cl)
{
	this->request_cancel(cl);
	prep_ctx.get()->m_pollfds.push_back(m_pimpl->m_done_event.get_poll());
}

void prepared_task::shadow_wait() throw()
{
	m_pimpl->m_done_event.wait();
}

void prepared_task::shadow_cancel_and_wait() throw()
{
	detail::scoped_pthread_lock l(m_pimpl->m_mutex);
	if (m_pimpl->m_runner)
		m_pimpl->m_runner->cancel_and_wait(this);
}

void prepared_task::attach_event_sink(prepared_task_event_sink & r) throw()
{
	this->addref();

	detail::scoped_pthread_lock l(m_pimpl->m_mutex);
	m_pimpl->m_runner = &r;
}

void prepared_task::detach_event_sink() throw()
{
	{
		detail::scoped_pthread_lock l(m_pimpl->m_mutex);
		m_pimpl->m_runner = 0;
	}

	this->release();
}

cancel_level prepared_task::requested_cancel_level() const throw()
{
	return cancel_level(m_pimpl->m_cl.load(std::memory_order_acquire));
}

void prepared_task::mark_finished() throw()
{
	m_pimpl->m_done_event.set();
}
