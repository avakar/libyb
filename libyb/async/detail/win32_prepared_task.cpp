#include "prepared_task.hpp"
#include "../runner.hpp"
#include "win32_runner_registry.hpp"
#include <stdexcept>
#include <windows.h>
using namespace yb;
using namespace yb::detail;

struct prepared_task_base::impl
	: public handle_completion_sink
{
	prepared_task_base * m_self;
	runner_registry * m_rr;
	cancel_level m_cl;
	refcount m_refcount;
	task_state_t m_state;
	HANDLE m_done_event;

	impl(prepared_task_base * self)
		: m_self(self), m_rr(nullptr), m_cl(cl_none), m_refcount(1), m_state(ts_stalled), m_done_event(0)
	{
		m_done_event = ::CreateEvent(0, TRUE, FALSE, 0);
		if (!m_done_event)
			throw "XXX";
	}

	~impl()
	{
		if (m_done_event)
			::CloseHandle(m_done_event);
	}

	void on_signaled(runner_registry & rr, HANDLE h) throw() override
	{
		m_self->on_nested_complete(rr);
	}
};

prepared_task_base::prepared_task_base()
	: m_pimpl(new impl(this))
{
}

prepared_task_base::~prepared_task_base()
{
}

void prepared_task_base::addref() throw()
{
	m_pimpl->m_refcount.addref();
}

void prepared_task_base::release() throw()
{
	if (m_pimpl->m_refcount.release() == 0)
		delete this;
}

void prepared_task_base::start(runner_registry & rr)
{
	assert(m_pimpl->m_rr == nullptr);
	m_pimpl->m_rr = &rr;

	this->addref();
	m_pimpl->m_rr->schedule_work(*this);
}

bool prepared_task_base::start_wait(runner_registry & rr)
{
	if (m_pimpl->m_state == ts_complete)
		return true;

	rr.add_handle(m_pimpl->m_done_event, *m_pimpl);
	return false;
}

void prepared_task_base::cancel_wait(cancel_level cl) throw()
{
	m_pimpl->m_cl = cl;
	m_pimpl->m_rr->schedule_work(*this);
}

cancel_level prepared_task_base::cl() const throw()
{
	return m_pimpl->m_cl;
}

void prepared_task_base::complete() throw()
{
	m_pimpl->m_state = ts_complete;
	::SetEvent(m_pimpl->m_done_event);
	this->release();
}

bool prepared_task_base::completed() const throw()
{
	return m_pimpl->m_state == ts_complete;
}

void prepared_task_base::do_work(runner_registry & rr) throw()
{
	if (m_pimpl->m_state == ts_stalled)
	{
		if (this->start_task(rr))
		{
			this->complete();
			return;
		}

		m_pimpl->m_state = ts_running;
	}

	if (this->cancel_task(m_pimpl->m_cl))
	{
		this->complete();
		return;
	}
}

