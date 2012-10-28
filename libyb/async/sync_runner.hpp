#ifndef LIBYB_ASYNC_SYNC_RUNNER_HPP
#define LIBYB_ASYNC_SYNC_RUNNER_HPP

#include "task.hpp"
#include "detail/win32_wait_context.hpp"
#include <utility> //move
#include <list>

namespace yb {

class sync_runner;

template <typename T>
class sync_promise
{
public:
	explicit sync_promise(sync_runner * runner)
		: m_runner(runner), m_refcount(1)
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

	void cancel(cancel_level_t cl)
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

	task_result<T> get();

private:
	sync_runner * m_runner;
	int m_refcount;
	task<T> m_task;

	friend class sync_runner;
};

template <typename T>
class sync_future
{
public:
	explicit sync_future(sync_promise<T> * promise)
		: m_promise(promise)
	{
		assert(m_promise);
		m_promise->addref();
	}

	explicit sync_future(std::exception_ptr exc)
		: m_promise(0), m_exception(std::move(exc))
	{
	}

	sync_future(sync_future const & o)
		: m_promise(o.m_promise), m_exception(o.m_exception)
	{
		if (m_promise)
			m_promise->addref();
	}

	~sync_future()
	{
		if (m_promise)
			m_promise->release();
	}

	sync_future & operator=(sync_future const & o)
	{
		if (o.m_promise)
			o.m_promise->addref();
		if (m_promise)
			m_promise->release();
		m_promise = o.m_promise;
		m_exception = o.m_exception;
		return *this;
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
		return m_promise->get().get();
	}

	void cancel(cancel_level_t cl = cancel_level_hint)
	{
		if (m_promise)
			m_promise->cancel(cl);
	}

private:
	sync_promise<T> * m_promise;
	std::exception_ptr m_exception;

	friend class sync_runner;
};

class sync_runner
{
public:
	~sync_runner()
	{
	}

	template <typename T>
	sync_future<T> post(task<T> && t)
	{
		try
		{
			std::unique_ptr<sync_promise<T>> promise(new sync_promise<T>(this));
			task<void> tt(new promise_task<T>(promise.get()));
			sync_promise<T> * ppromise = promise.release();
			m_parallel_tasks |= std::move(tt);
			ppromise->set_task(std::move(t));
			return sync_future<T>(ppromise);
		}
		catch (...)
		{
			return sync_future<T>(std::current_exception());
		}
	}

	template <typename T>
	task_result<T> try_run(sync_future<T> const & f);

	template <typename T>
	task_result<T> try_run(task<T> && t)
	{
		sync_future<T> f = this->post(std::move(t));
		return this->try_run(f);
	}

	template <typename T>
	T run(task<T> && t)
	{
		return this->try_run(std::move(t)).get();
	}

private:
	template <typename T>
	class promise_task
		: public task_base<void>
	{
	public:
		promise_task(sync_promise<T> * promise)
			: m_promise(promise)
		{
		}

		~promise_task()
		{
			m_promise->release();
		}

		void cancel(cancel_level_t cl) throw()
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
				return async::value();
			return nulltask;
		}

	private:
		sync_promise<T> * m_promise;
	};

	task<void> m_parallel_tasks;
};

template <typename T>
task_result<T> sync_promise<T>::get()
{
	return m_runner->try_run(sync_future<T>(this));
}

template <typename T>
task_result<T> sync_runner::try_run(sync_future<T> const & f)
{
	sync_promise<T> * promise = f.m_promise;
	if (!promise)
		return task_result<T>(f.m_exception);

	try
	{
		task_wait_preparation_context wait_ctx;
		task_wait_preparation_context_impl & wait_ctx_impl = *wait_ctx.get();

		assert(!promise->m_task.empty());
		while (!promise->m_task.has_result())
		{
			wait_ctx.clear();
			m_parallel_tasks.prepare_wait(wait_ctx);

			if (wait_ctx_impl.m_finished_tasks)
			{
				task_wait_finalization_context finish_ctx;
				finish_ctx.finished_tasks = wait_ctx_impl.m_finished_tasks;
				m_parallel_tasks.finish_wait(finish_ctx);
			}
			else
			{
				assert(!wait_ctx_impl.m_handles.empty());

				DWORD dwRes = WaitForMultipleObjects(wait_ctx_impl.m_handles.size(), wait_ctx_impl.m_handles.data(), FALSE, INFINITE);
				assert(dwRes >= WAIT_OBJECT_0 && dwRes < WAIT_OBJECT_0 + wait_ctx_impl.m_handles.size());

				task_wait_finalization_context finish_ctx;
				finish_ctx.finished_tasks = false;
				finish_ctx.selected_poll_item = dwRes - WAIT_OBJECT_0;
				m_parallel_tasks.finish_wait(finish_ctx);
			}
		}

		return promise->m_task.get_result();
	}
	catch (...)
	{
		return std::current_exception();
	}
}

template <typename T>
task_result<T> try_run(task<T> && t)
{
	sync_runner runner;
	return runner.try_run(std::move(t));
}

template <typename T>
T run(task<T> && t)
{
	task_result<T> r = try_run(std::move(t));
	return r.get();
}

} // namespace yb

#endif // LIBYB_ASYNC_SYNC_RUNNER_HPP
