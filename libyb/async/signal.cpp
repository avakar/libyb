#include "signal.hpp"

namespace yb {

struct signal::impl
{
	impl()
		: m_signalled(false), m_refcount(2)
	{
	}

	void addref()
	{
		++m_refcount;
	}

	void release()
	{
		if (--m_refcount == 0)
			delete this;
	}

	bool m_signalled;
	int m_refcount;
};

struct signal::signal_task
	: task_base<void>
{
	signal_task(impl * pimpl)
		: m_pimpl(pimpl)
	{
	}

	~signal_task()
	{
		m_pimpl->release();
	}

	void cancel() throw()
	{
	}

	task_result<void> wait() throw()
	{
		return task_result<void>();
	}

	void prepare_wait(task_wait_preparation_context & ctx)
	{
		if (m_pimpl->m_signalled)
			ctx.set_finished();
	}

	task<void> finish_wait(task_wait_finalization_context & ctx) throw()
	{
		return make_value_task();
	}

	impl * m_pimpl;
};

signal::signal()
	: m_pimpl(0)
{
}

signal::signal(signal const & o)
	: m_pimpl(o.m_pimpl)
{
	m_pimpl->addref();
}

signal::signal(signal && o)
	: m_pimpl(o.m_pimpl)
{
	o.m_pimpl = 0;
}

signal::~signal()
{
	m_pimpl->release();
}

void signal::fire()
{
	assert(m_pimpl);
	m_pimpl->m_signalled = true;
}

task<void> wait_for(signal & sig)
{
	assert(!sig.m_pimpl);

	try
	{
		std::unique_ptr<signal::impl> pimpl(new signal::impl());
		std::unique_ptr<signal::signal_task> ptask(new signal::signal_task(pimpl.get()));

		sig.m_pimpl = pimpl.release();
		return task<void>(ptask.release());
	}
	catch (...)
	{
		return make_value_task<void>(std::current_exception());
	}
}

} // namespace yb
