#ifndef LIBYB_ASYNC_DETAIL_CANCELLATION_TOKEN_TASK_HPP
#define LIBYB_ASYNC_DETAIL_CANCELLATION_TOKEN_TASK_HPP

#include "../task_base.hpp"
#include "../cancel_exception.hpp"
#include "../../utils/noncopyable.hpp"

namespace yb {
namespace detail {

class cancellation_token_core_base
	: noncopyable
{
public:
	cancellation_token_core_base()
		: m_refcount(0)
	{
	}

	virtual ~cancellation_token_core_base()
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

	virtual void cancel(cancel_level cl) = 0;

private:
	int m_refcount;
};

template <typename R>
struct cancellation_token_core
	: public cancellation_token_core_base
{
	task<R> m_task;

	void cancel(cancel_level cl)
	{
		m_task.cancel(cl);
	}
};

template <typename R>
class cancellation_token_task
	: public task_base<R>, noncopyable
{
public:
	explicit cancellation_token_task(cancellation_token_core<R> * core)
		: m_core(core)
	{
		assert(m_core);
		m_core->addref();
	}

	~cancellation_token_task()
	{
		m_core->release();
	}

	void cancel(cancel_level cl) throw()
	{
		m_core->m_task.cancel(cl);
	}

	task_result<R> cancel_and_wait() throw()
	{
		return m_core->m_task.cancel_and_wait();
	}

	void prepare_wait(task_wait_preparation_context & ctx)
	{
		m_core->m_task.prepare_wait(ctx);
	}

	task<R> finish_wait(task_wait_finalization_context & ctx) throw()
	{
		m_core->m_task.finish_wait(ctx);
		if (m_core->m_task.has_result())
			return std::move(m_core->m_task);
		return nulltask;
	}

private:
	cancellation_token_core<R> * m_core;
};

template <>
class cancellation_token_task<void>
	: public task_base<void>, noncopyable
{
public:
	cancellation_token_task(cancellation_token_core<void> * core, bool catch_cancel = false)
		: m_core(core), m_catch_cancel(catch_cancel)
	{
		assert(m_core);
		m_core->addref();
	}

	~cancellation_token_task()
	{
		m_core->release();
	}

	void cancel(cancel_level cl) throw()
	{
		m_core->m_task.cancel(cl);
	}

	task_result<void> cancel_and_wait() throw()
	{
		task_result<void> r = m_core->m_task.cancel_and_wait();
		if (m_catch_cancel && r.has_exception())
		{
			try
			{
				r.rethrow();
			}
			catch (task_cancelled const &)
			{
				return task_result<void>();
			}
			catch (...)
			{
				return task_result<void>(std::current_exception());
			}
		}
		return std::move(r);
	}

	void prepare_wait(task_wait_preparation_context & ctx)
	{
		m_core->m_task.prepare_wait(ctx);
	}

	task<void> finish_wait(task_wait_finalization_context & ctx) throw()
	{
		m_core->m_task.finish_wait(ctx);
		if (m_core->m_task.has_result())
		{
			task_result<void> r = m_core->m_task.get_result();
			if (m_catch_cancel && r.has_exception())
			{
				try
				{
					r.rethrow();
				}
				catch (task_cancelled const &)
				{
					return async::value();
				}
				catch (...)
				{
					return async::raise<void>();
				}
			}
			return async::result(std::move(r));
		}
		return nulltask;
	}

private:
	cancellation_token_core<void> * m_core;
	bool m_catch_cancel;
};

} // namespace detail
} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_CANCELLATION_TOKEN_TASK_HPP
