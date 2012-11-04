#ifndef LIBYB_ASYNC_DETAIL_CANCEL_LEVEL_UPGRADE_TASK_HPP
#define LIBYB_ASYNC_DETAIL_CANCEL_LEVEL_UPGRADE_TASK_HPP

#include "../task_base.hpp"
#include "../cancel_exception.hpp"
#include "../task.hpp"

namespace yb {
namespace detail {

template <typename R>
class cancel_level_upgrade_task
	: public task_base<R>
{
public:
	cancel_level_upgrade_task(task<R> && nested, cancel_level from, cancel_level to)
		: m_nested(std::move(nested)), m_from(from), m_to(to)
	{
	}

	void cancel(cancel_level cl) throw()
	{
		if (cl >= m_from && cl < m_to)
			cl = m_to;

		m_nested.cancel(cl);
	}

	task_result<R> cancel_and_wait() throw()
	{
		return m_nested.cancel_and_wait();
	}

	void prepare_wait(task_wait_preparation_context & ctx)
	{
		m_nested.prepare_wait(ctx);
	}

	task<R> finish_wait(task_wait_finalization_context & ctx) throw()
	{
		m_nested.finish_wait(ctx);
		if (m_nested.has_task())
			return nulltask;
		return std::move(m_nested);
	}

private:
	task<R> m_nested;
	cancel_level m_from;
	cancel_level m_to;
};

template <>
class cancel_level_upgrade_task<void>
	: public task_base<void>
{
public:
	cancel_level_upgrade_task(task<void> && nested, cancel_level from, cancel_level to, bool catch_cancel = false)
		: m_nested(std::move(nested)), m_from(from), m_to(to), m_catch_cancel(catch_cancel)
	{
	}

	void cancel(cancel_level cl) throw()
	{
		if (cl >= m_from && cl < m_to)
			cl = m_to;

		m_nested.cancel(cl);
	}

	task_result<void> cancel_and_wait() throw()
	{
		return m_nested.cancel_and_wait();
	}

	void prepare_wait(task_wait_preparation_context & ctx)
	{
		m_nested.prepare_wait(ctx);
	}

	task<void> finish_wait(task_wait_finalization_context & ctx) throw()
	{
		m_nested.finish_wait(ctx);
		if (m_nested.has_task())
			return nulltask;

		task_result<void> r = m_nested.get_result();

		try
		{
			r.rethrow();
			return async::value();
		}
		catch (task_cancelled const &)
		{
			if (m_catch_cancel)
				return async::value();
			return async::raise<void>();
		}
		catch (...)
		{
			return async::raise<void>();
		}
	}

private:
	task<void> m_nested;
	cancel_level m_from;
	cancel_level m_to;
	bool m_catch_cancel;
};

} // namespace detail
} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_CANCEL_LEVEL_UPGRADE_TASK_HPP
