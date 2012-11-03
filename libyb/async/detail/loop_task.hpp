#ifndef LIBYB_ASYNC_DETAIL_LOOP_TASK_HPP
#define LIBYB_ASYNC_DETAIL_LOOP_TASK_HPP

#include "../task_base.hpp"
#include "../task.hpp"
#include "wait_context.hpp"

namespace yb {
namespace detail {

template <typename S, typename F>
class loop_task
	: public task_base<void>
{
public:
	loop_task(task<S> && t, F const & f);

	void cancel(cancel_level cl) throw();
	task_result<void> cancel_and_wait() throw();
	void prepare_wait(task_wait_preparation_context & ctx);
	task<void> finish_wait(task_wait_finalization_context & ctx) throw();

private:
	task<S> m_task;
	F m_f;
	cancel_level m_cancel_level;
};

template <typename F>
class loop_task<void, F>
	: public task_base<void>
{
public:
	loop_task(task<void> && t, F const & f);

	void cancel(cancel_level cl) throw();
	task_result<void> cancel_and_wait() throw();
	void prepare_wait(task_wait_preparation_context & ctx);
	task<void> finish_wait(task_wait_finalization_context & ctx) throw();

private:
	task<void> m_task;
	F m_f;
	cancel_level m_cancel_level;
};

} // namespace detail
} // namespace yb

namespace yb {
namespace detail {

template <typename S, typename F>
loop_task<S, F>::loop_task(task<S> && t, F const & f)
	: m_task(std::move(t)), m_f(f), m_cancel_level(cl_none)
{
}

template <typename S, typename F>
void loop_task<S, F>::cancel(cancel_level cl) throw()
{
	m_cancel_level = (std::max)(m_cancel_level, cl);
	m_task.cancel(cl);
}

template <typename S, typename F>
task_result<void> loop_task<S, F>::cancel_and_wait() throw()
{
	while (!m_task.empty())
	{
		task_result<S> r = m_task.cancel_and_wait();
		if (r.has_exception())
			return r.exception();

		try
		{
			m_task = m_f(r.get(), cl_kill);
		}
		catch (...)
		{
			return task_result<void>(std::current_exception());
		}
	}

	return task_result<void>();
}

template <typename S, typename F>
void loop_task<S, F>::prepare_wait(task_wait_preparation_context & ctx)
{
	m_task.prepare_wait(ctx);
}

template <typename S, typename F>
task<void> loop_task<S, F>::finish_wait(task_wait_finalization_context & ctx) throw()
{
	m_task.finish_wait(ctx);
	while (m_task.has_result())
	{
		task_result<S> r = m_task.get_result();
		if (r.has_exception())
			return async::raise<void>(r.exception());

		try
		{
			m_task = m_f(r.get(), m_cancel_level);
		}
		catch (...)
		{
			return async::raise<void>();
		}

		if (m_task.empty())
			return async::value();
	}

	return nulltask;
}

template <typename F>
loop_task<void, F>::loop_task(task<void> && t, F const & f)
	: m_task(std::move(t)), m_f(f), m_cancel_level(cl_none)
{
}

template <typename F>
void loop_task<void, F>::cancel(cancel_level cl) throw()
{
	m_cancel_level = (std::max)(m_cancel_level, cl);
	m_task.cancel(cl);
}

template <typename F>
task_result<void> loop_task<void, F>::cancel_and_wait() throw()
{
	while (!m_task.empty())
	{
		task_result<void> r = m_task.cancel_and_wait();
		if (r.has_exception())
			return r;

		try
		{
			m_task = m_f(cl_kill);
		}
		catch (...)
		{
			return task_result<void>(std::current_exception());
		}
	}

	return task_result<void>();
}

template <typename F>
void loop_task<void, F>::prepare_wait(task_wait_preparation_context & ctx)
{
	assert(!m_task.empty());

	if (m_task.has_result())
		ctx.set_finished();
	else
		m_task.prepare_wait(ctx);
}

template <typename F>
task<void> loop_task<void, F>::finish_wait(task_wait_finalization_context & ctx) throw()
{
	assert(!m_task.empty());

	if (!m_task.has_result())
		m_task.finish_wait(ctx);

	while (m_task.has_result())
	{
		task_result<void> r = m_task.get_result();
		if (r.has_exception())
			return async::result(std::move(r));

		try
		{
			m_task = m_f(m_cancel_level);
		}
		catch (...)
		{
			return async::raise<void>();
		}

		if (m_task.empty())
			return async::value();
	}

	return nulltask;
}

} // namespace detail
} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_LOOP_TASK_HPP
