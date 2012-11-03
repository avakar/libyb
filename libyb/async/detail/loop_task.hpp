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

} // namespace detail
} // namespace yb

namespace yb {
namespace detail {

template <typename R, typename F>
task<R> invoke_loop_body(F & f, task_result<R> && r, cancel_level cl)
{
	try
	{
		return f(r.get(), cl);
	}
	catch (...)
	{
		return async::raise<R>();
	}
}

template <typename F>
task<void> invoke_loop_body(F & f, task_result<void> &&, cancel_level cl)
{
	try
	{
		return f(cl);
	}
	catch (...)
	{
		return async::raise<void>();
	}
}

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
		m_task = invoke_loop_body(m_f, std::move(r), cl_kill);
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
		m_task = invoke_loop_body(m_f, std::move(r), m_cancel_level);
		if (m_task.empty())
			return async::value();
	}

	return nulltask;
}

} // namespace detail

template <typename S, typename F>
task<void> loop(task<S> && t, F f)
{
	while (t.has_result())
	{
		task_result<S> r = t.get_result();
		if (r.has_exception())
			return async::raise<void>(r.exception());
		t = detail::invoke_loop_body(f, std::move(r), cl_none);
		if (t.empty())
			return async::value();
	}

	try
	{
		return task<void>(new detail::loop_task<S, F>(std::move(t), f));
	}
	catch (...)
	{
		while (!t.empty())
		{
			task_result<S> r = t.cancel_and_wait();
			if (r.has_exception())
				return async::raise<void>(r.exception());
			t = detail::invoke_loop_body(f, std::move(r), cl_none);
		}

		return async::raise<void>();
	}
}

template <typename F>
task<void> loop(F && f)
{
	return loop(async::value(), std::move(f));
}

} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_LOOP_TASK_HPP
