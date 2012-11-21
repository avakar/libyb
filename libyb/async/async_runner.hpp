#ifndef LIBYB_ASYNC_ASYNC_RUNNER_HPP
#define LIBYB_ASYNC_ASYNC_RUNNER_HPP

#include "task.hpp"
#include "../utils/noncopyable.hpp"
#include <memory>
#include <utility>

namespace yb {

class async_runner;

namespace detail {

class async_promise_base
	: noncopyable
{
public:
	explicit async_promise_base(async_runner * runner);
	virtual ~async_promise_base();

	void addref();
	void release();

	void mark_finished();
	void wait();
	void cancel(cancel_level cl);
	void perform_pending_cancels();

	virtual void prepare_wait(task_wait_preparation_context & ctx) = 0;
	virtual bool finish_wait(task_wait_finalization_context & ctx) throw() = 0;
	virtual void cancel_and_wait() = 0;

protected:
	virtual void do_cancel(cancel_level cl) = 0;

private:
	struct impl;
	std::unique_ptr<impl> m_pimpl;
};

template <typename T>
class async_promise
	: public async_promise_base
{
public:
	explicit async_promise(async_runner * runner)
		: async_promise_base(runner)
	{
	}

	void set_task(task<T> && t)
	{
		m_task = std::move(t);
	}

	void prepare_wait(task_wait_preparation_context & ctx)
	{
		m_task.prepare_wait(ctx);
	}

	bool finish_wait(task_wait_finalization_context & ctx) throw()
	{
		m_task.finish_wait(ctx);
		return this->has_result();
	}

	void cancel_and_wait()
	{
		m_task.cancel_and_wait();
	}

	bool has_result() const
	{
		return m_task.has_result();
	}

	task_result<T> get()
	{
		this->wait();

		assert(m_task.has_result());
		return m_task.get_result();
	}

protected:
	void do_cancel(cancel_level cl)
	{
		m_task.cancel(cl);
	}

private:
	task<T> m_task;
};

}

template <typename T>
class async_future
{
public:
	explicit async_future(detail::async_promise<T> * promise = 0)
		: m_promise(promise)
	{
		if (m_promise)
			m_promise->addref();
	}

	explicit async_future(std::exception_ptr exc)
		: m_promise(0), m_exception(std::move(exc))
	{
	}

	async_future(async_future && o)
		: m_promise(o.m_promise), m_exception(std::move(o.m_exception))
	{
		o.m_promise = 0;
	}

	~async_future()
	{
		if (m_promise)
		{
			m_promise->cancel(cl_kill);
			m_promise->wait();
			m_promise->release();
		}
	}

	async_future & operator=(async_future && o)
	{
		if (m_promise)
			m_promise->release();
		m_promise = o.m_promise;
		o.m_promise = 0;
		m_exception = std::move(o.m_exception);
		return *this;
	}

	bool empty() const
	{
		return m_promise == 0;
	}

	void detach()
	{
		if (m_promise)
		{
			m_promise->release();
			m_promise = 0;
		}
	}

	task_result<T> try_get()
	{
		if (m_promise)
			return m_promise->get();
		else
			return task_result<T>(m_exception);
	}

	T get()
	{
		return this->try_get().get();
	}

	void cancel(cancel_level cl = cl_abort)
	{
		if (m_promise)
			m_promise->cancel(cl);
	}

private:
	detail::async_promise<T> * m_promise;
	std::exception_ptr m_exception;

	async_future(async_future const &);
	async_future & operator=(async_future const &);

	friend class async_runner;
};

class async_runner
	: noncopyable
{
public:
	async_runner();
	~async_runner();

	template <typename T>
	async_future<T> post(task<T> && t)
	{
		assert(!t.empty());

		try
		{
			std::unique_ptr<detail::async_promise<T>> promise(new detail::async_promise<T>(this));
			if (promise->has_result())
			{
				promise->set_task(std::move(t));
				return async_future<T>(promise.release());
			}

			submit_context sc(*this);
			promise->set_task(std::move(t));
			sc.submit(promise.get());
			return async_future<T>(promise.release());
		}
		catch (...)
		{
			return async_future<T>(std::current_exception());
		}
	}

	template <typename T>
	void post_detached(task<T> && t)
	{
		async_future<T>(this->post(std::move(t))).detach();
	}

	template <typename T>
	task_result<T> try_run(async_future<T> & f)
	{
		return f.try_get();
	}

	template <typename T>
	task_result<T> try_run(task<T> && t)
	{
		async_future<T> f = this->post(std::move(t));
		return this->try_run(f);
	}

	template <typename T>
	T run(task<T> && t)
	{
		return this->try_run(std::move(t)).get();
	}

private:
	struct submit_context
		: noncopyable
	{
		submit_context(async_runner & runner);
		~submit_context();
		void submit(detail::async_promise_base * p);
		async_runner & m_runner;
	};

	struct impl;
	std::unique_ptr<impl> m_pimpl;

	friend class detail::async_promise_base;
};

} // namespace yb

#endif // LIBYB_ASYNC_ASYNC_RUNNER_HPP
