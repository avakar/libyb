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
	async_promise_base();
	~async_promise_base();

	void mark_finished();
	void wait();

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
		: m_runner(runner), m_refcount(0)
	{
	}

	void addref()
	{
		++m_refcount;
	}

	void release()
	{
		if (!--m_refcount)
			delete this;
	}

	void cancel(cancel_level cl)
	{
		m_task.cancel(cl);
	}

	void cancel_and_wait()
	{
		m_task = async::result(m_task.cancel_and_wait());
	}

	void set_task(task<T> && t)
	{
		m_task = std::move(t);
	}

	void prepare_wait(task_wait_preparation_context & ctx)
	{
		m_task.prepare_wait(ctx);
	}

	void finish_wait(task_wait_finalization_context & ctx) throw()
	{
		m_task.finish_wait(ctx);
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

private:
	async_runner * m_runner;
	int m_refcount;
	task<T> m_task;
};

}

template <typename T>
class async_future
{
public:
	explicit async_future(detail::async_promise<T> * promise)
		: m_promise(promise)
	{
		assert(m_promise);
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
			m_promise->cancel_and_wait();
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

	T get(cancel_level cl = cl_none)
	{
		if (cl != cl_none)
			this->cancel(cl);
		return m_promise->get().get();
	}

	void cancel(cancel_level cl)
	{
		if (m_promise)
			m_promise->cancel(cl);
	}

	T wait(cancel_level cl = cl_none)
	{
		return this->get(cl);
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

	void start();

	template <typename T>
	async_future<T> post(task<T> && t)
	{
		assert(!t.empty());

		try
		{
			std::unique_ptr<detail::async_promise<T>> promise(new detail::async_promise<T>(this));
			if (t.has_result())
			{
				promise->set_task(std::move(t));
				return async_future<T>(promise.release());
			}

			task<void> tt(new promise_task<T>(promise.get()));
			detail::async_promise<T> * ppromise = promise.release();
			this->submit(std::move(tt));
			ppromise->set_task(std::move(t));
			return async_future<T>(ppromise);
		}
		catch (...)
		{
			return async_future<T>(std::current_exception());
		}
	}

	template <typename T>
	task_result<T> try_run(async_future<T> & f);


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
	void submit(task<void> && t);

	template <typename T>
	class promise_task
		: public task_base<void>
	{
	public:
		promise_task(detail::async_promise<T> * promise)
			: m_promise(promise)
		{
			m_promise->addref();
		}

		~promise_task()
		{
			m_promise->release();
		}

		void cancel(cancel_level cl) throw()
		{
			m_promise->cancel(cl);
		}

		task_result<void> cancel_and_wait() throw()
		{
			m_promise->cancel_and_wait();
			assert(m_promise->has_result());
			return task_result<void>();
		}

		void prepare_wait(task_wait_preparation_context & ctx)
		{
			m_promise->prepare_wait(ctx);
		}

		task<void> finish_wait(task_wait_finalization_context & ctx) throw()
		{
			m_promise->finish_wait(ctx);
			if (m_promise->has_result())
			{
				m_promise->mark_finished();
				return async::value();
			}
			return nulltask;
		}

	private:
		detail::async_promise<T> * m_promise;
	};

	struct impl;
	std::unique_ptr<impl> m_pimpl;
};

template <typename T>
task_result<T> async_runner::try_run(async_future<T> & f)
{
	return f.try_get();
}

} // namespace yb

#endif // LIBYB_ASYNC_ASYNC_RUNNER_HPP
