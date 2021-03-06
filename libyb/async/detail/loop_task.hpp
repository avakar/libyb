#ifndef LIBYB_ASYNC_DETAIL_LOOP_TASK_HPP
#define LIBYB_ASYNC_DETAIL_LOOP_TASK_HPP

#include "../task_base.hpp"
#include "../task.hpp"
#include "wait_context.hpp"

namespace yb {
namespace detail {

template <typename T>
struct loop_state
{
	loop_state();
	loop_state(loop_state<T> && o);

	template <typename U>
	explicit loop_state(U && state);

	T m_state;
};

template <>
struct loop_state<void>
{
};

template <typename S, typename F, typename T>
class loop_task
	: public task_base<void>, private loop_state<T>
{
public:
	loop_task(task<S> && t, F const & f, loop_state<T> && state);

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

template <typename R, typename F, typename T>
task<R> invoke_loop_body2(F & f, task_result<R> && r, loop_state<T> & state, cancel_level cl)
{
	return f(r.get(), state.m_state, cl);
}

template <typename F, typename T>
task<void> invoke_loop_body2(F & f, task_result<void> &&, loop_state<T> & state, cancel_level cl)
{
	return f(state.m_state, cl);
}

template <typename R, typename F>
task<R> invoke_loop_body2(F & f, task_result<R> && r, loop_state<void> &, cancel_level cl)
{
	return f(r.get(), cl);
}

template <typename F>
task<void> invoke_loop_body2(F & f, task_result<void> &&, loop_state<void> &, cancel_level cl)
{
	return f(cl);
}

template <typename R, typename F, typename T>
task<R> invoke_loop_body(F & f, task_result<R> && r, loop_state<T> & state, cancel_level cl)
{
	try
	{
		return invoke_loop_body2(f, std::move(r), state, cl);
	}
	catch (...)
	{
		return async::raise<R>();
	}
}

template <typename T>
loop_state<T>::loop_state()
	: m_state()
{
}

template <typename T>
loop_state<T>::loop_state(loop_state<T> && o)
	: m_state(std::move(o.m_state))
{
}

template <typename T>
template <typename U>
loop_state<T>::loop_state(U && state)
	: m_state(std::forward<U>(state))
{
}

template <typename S, typename F, typename T>
loop_task<S, F, T>::loop_task(task<S> && t, F const & f, loop_state<T> && state)
	: loop_state<T>(std::move(state)), m_task(std::move(t)), m_f(f), m_cancel_level(cl_none)
{
}

template <typename S, typename F, typename T>
void loop_task<S, F, T>::cancel(cancel_level cl) throw()
{
	m_cancel_level = (std::max)(m_cancel_level, cl);
	m_task.cancel(cl);
}

template <typename S, typename F, typename T>
task_result<void> loop_task<S, F, T>::cancel_and_wait() throw()
{
	while (!m_task.empty())
	{
		task_result<S> r = m_task.cancel_and_wait();
		if (r.has_exception())
			return r.exception();
		m_task = invoke_loop_body(m_f, std::move(r), *this, cl_kill);
	}

	return task_result<void>();
}

template <typename S, typename F, typename T>
void loop_task<S, F, T>::prepare_wait(task_wait_preparation_context & ctx)
{
	m_task.prepare_wait(ctx);
}

template <typename S, typename F, typename T>
task<void> loop_task<S, F, T>::finish_wait(task_wait_finalization_context & ctx) throw()
{
	m_task.finish_wait(ctx);
	while (m_task.has_result())
	{
		task_result<S> r = m_task.get_result();
		if (r.has_exception())
			return async::raise<void>(r.exception());
		m_task = invoke_loop_body(m_f, std::move(r), *this, m_cancel_level);
		if (m_task.empty())
			return async::value();
	}

	return nulltask;
}

template <typename S, typename T, typename F>
task<void> loop(task<S> && t, loop_state<T> & state, F && f)
{
	while (t.has_result())
	{
		task_result<S> r = t.get_result();
		if (r.has_exception())
			return async::raise<void>(r.exception());
		t = detail::invoke_loop_body(f, std::move(r), state, cl_none);
		if (t.empty())
			return async::value();
	}

	try
	{
		return task<void>(new detail::loop_task<S, F, T>(std::move(t), std::move(f), std::move(state)));
	}
	catch (...)
	{
		while (!t.empty())
		{
			task_result<S> r = t.cancel_and_wait();
			if (r.has_exception())
				return async::raise<void>(r.exception());
			t = detail::invoke_loop_body(f, std::move(r), state, cl_none);
		}

		return async::raise<void>();
	}
}

} // namespace detail

template <typename S, typename F>
task<void> loop(task<S> && t, F f)
{
	detail::loop_state<void> state;
	return detail::loop(std::move(t), state, std::move(f));
}

template <typename S, typename T, typename F, typename U>
task<void> loop_with_state(task<S> && t, U && s, F f)
{
	detail::loop_state<T> state(std::move(s));
	return detail::loop(std::move(t), state, std::move(f));
}

template <typename S, typename T, typename F>
task<void> loop_with_state(task<S> && t, F f)
{
	detail::loop_state<T> state;
	return detail::loop(std::move(t), state, std::move(f));
}

template <typename F>
task<void> loop(F && f)
{
	return loop(async::value(), std::move(f));
}

} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_LOOP_TASK_HPP
