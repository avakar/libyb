#ifndef LIBYB_ASYNC_DETAIL_PREPARED_TASK_HPP
#define LIBYB_ASYNC_DETAIL_PREPARED_TASK_HPP

#include "../task.hpp"
#include "../../utils/refcount.hpp"
#include <memory>

namespace yb {

class runner;

namespace detail {

class prepared_task_base
	: protected runner_work
{
public:
	void addref() throw();
	void release() throw();

	void start(runner_registry & rr);

	bool completed() const throw();

protected:
	enum task_state_t { ts_stalled, ts_running, ts_complete };

	prepared_task_base();
	~prepared_task_base();

	cancel_level cl() const throw();

	bool start_wait(runner_registry & rr);
	void cancel_wait(cancel_level cl) throw();

	virtual bool start_task(runner_registry & rr) = 0;
	virtual bool cancel_task(cancel_level cl) throw() = 0;
	void do_work(runner_registry & rr) override;

	void complete() throw();

	virtual void on_nested_complete(runner_registry & rr) = 0;

private:
	struct impl;
	std::unique_ptr<impl> m_pimpl;
};

template <typename R>
class prepared_task
	: public prepared_task_base
	, public task_base<R>
	, private task_completion_sink<R>
{
public:
	explicit prepared_task(task<R> && t)
		: m_task(std::move(t))
	{
	}

	task<R> make_shadow()
	{
		this->addref();
		return task<R>::from_task(this);
	}

	task<R> get_result()
	{
		assert(!m_task.has_task());
		return std::move(m_task);
	}

	static task<R> create(runner & r, task<R> && t)
	{
		prepared_task<R> * p = new prepared_task<R>(r, std::move(t));
		p->addref();
		return task<R>::from_task(p);
	}

	task<R> wait() throw()
	{
		this->wait_wait();
		return std::move(m_task);
	}

private:
	void release() throw() override
	{
		this->prepared_task_base::release();
	}

	void on_nested_complete(runner_registry & rr) override
	{
		m_sink->on_completion(rr, std::move(m_task));
	}

	task<R> start(runner_registry & rr, task_completion_sink<R> & sink) override
	{
		assert(m_sink == nullptr);
		m_sink = &sink;
		this->start_wait(rr);
		return yb::nulltask;
	}

	task<R> cancel(runner_registry * rr, cancel_level cl) throw() override
	{
		(void)rr;
		this->cancel_wait(cl);
		return yb::nulltask;
	}

	bool start_task(runner_registry & rr) override
	{
		return m_task.start(rr, *this);
	}

	bool cancel_task(cancel_level cl) throw() override
	{
		return m_task.cancel(cl);
	}

	void on_completion(runner_registry & rr, task<R> && r) override
	{
		if (m_task.replace(rr, *this, std::move(r)))
			this->complete();
	}

	task<R> m_task;
	task_completion_sink<R> * m_sink;
};

} // namespace detail
} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_PREPARED_TASK_HPP
