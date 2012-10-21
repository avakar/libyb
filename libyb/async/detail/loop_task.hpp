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

	void cancel() throw();
	task_result<void> wait() throw();
	void prepare_wait(task_wait_preparation_context & ctx);
	task<void> finish_wait(task_wait_finalization_context & ctx) throw();

private:
	task<S> m_task;
	F m_f;
};

template <typename F>
class loop_task<void, F>
	: public task_base<void>
{
public:
	loop_task(task<void> && t, F const & f);

	void cancel() throw();
	task_result<void> wait() throw();
	void prepare_wait(task_wait_preparation_context & ctx);
	task<void> finish_wait(task_wait_finalization_context & ctx) throw();

private:
	task<void> m_task;
	F m_f;
};

} // namespace detail
} // namespace yb

namespace yb {
namespace detail {

template <typename S, typename F>
loop_task<S, F>::loop_task(task<S> && t, F const & f)
	: m_task(std::move(t)), m_f(f)
{
}

template <typename S, typename F>
void loop_task<S, F>::cancel() throw()
{
	m_task.cancel();
}

template <typename S, typename F>
task_result<void> loop_task<S, F>::wait() throw()
{
	while (!m_task.empty())
	{
		try
		{
			m_task = m_f(m_task.wait().get());
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
	if (m_task.has_result())
	{
		try
		{
			m_task = m_f(m_task.get_result().get());
		}
		catch (...)
		{
			return make_value_task<void>(std::current_exception());
		}

		if (m_task.empty())
			return make_value_task();
	}

	return nulltask;
}

template <typename F>
loop_task<void, F>::loop_task(task<void> && t, F const & f)
	: m_task(std::move(t)), m_f(f)
{
}

template <typename F>
void loop_task<void, F>::cancel() throw()
{
	m_task.cancel();
}

template <typename F>
task_result<void> loop_task<void, F>::wait() throw()
{
	while (!m_task.empty())
	{
		try
		{
			m_task.wait().rethrow();
			m_task = m_f();
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

	if (m_task.has_result())
	{
		try
		{
			m_task.get_result().rethrow();
			m_task = m_f();
		}
		catch (...)
		{
			return make_value_task<void>(std::current_exception());
		}

		if (m_task.empty())
			return make_value_task();
	}

	return nulltask;
}

} // namespace detail
} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_LOOP_TASK_HPP
