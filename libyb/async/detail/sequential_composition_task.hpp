#ifndef LIBYB_ASYNC_DETAIL_SEQUENTION_COMPOSITION_TASK_HPP
#define LIBYB_ASYNC_DETAIL_SEQUENTION_COMPOSITION_TASK_HPP

#include "../task_base.hpp"
#include "task_fwd.hpp"
#include <memory> // unique_ptr
#include <cassert>
#include <exception> // exception_ptr

namespace yb {
namespace detail {

template <typename R, typename S, typename F>
class sequential_composition_task
	: public task_base<R>
{
public:
	sequential_composition_task(task<S> && task, F next);

	void cancel(cancel_level cl) throw();
	task_result<R> cancel_and_wait() throw();

	void prepare_wait(task_wait_preparation_context & ctx);
	task<R> finish_wait(task_wait_finalization_context & ctx) throw();

private:
	task<S> m_task;
	F m_next;
};

} // namespace detail
} // namespace yb


namespace yb {
namespace detail {

template <typename R, typename S, typename F>
sequential_composition_task<R, S, F>::sequential_composition_task(task<S> && task, F next)
	: m_next(std::move(next))
{
	assert(!task.has_result() && !task.empty());
	m_task = std::move(task);
}

template <typename R, typename S, typename F>
void sequential_composition_task<R, S, F>::cancel(cancel_level cl) throw()
{
	m_task.cancel(cl);
}

template <typename R, typename S, typename F>
task_result<R> sequential_composition_task<R, S, F>::cancel_and_wait() throw()
{
	task_result<S> s = m_task.cancel_and_wait();
	try
	{
		task<R> r = m_next(s);
		if (r.has_task())
			return r.cancel_and_wait();

		assert(r.has_result());
		return r.get_result();
	}
	catch (...)
	{
		return task_result<R>(std::current_exception());
	}
}

template <typename R, typename S, typename F>
void sequential_composition_task<R, S, F>::prepare_wait(task_wait_preparation_context & ctx)
{
	assert(m_task.has_task());
	m_task.prepare_wait(ctx);
}

template <typename R, typename S, typename F>
task<R> sequential_composition_task<R, S, F>::finish_wait(task_wait_finalization_context & ctx) throw()
{
	m_task.finish_wait(ctx);

	if (m_task.has_result())
	{
		try
		{
			task_result<S> s = m_task.get_result();
			return m_next(std::move(s));
		}
		catch (...)
		{
			return async::raise<R>();
		}
	}

	return nulltask;
}

}
}

#endif // LIBYB_ASYNC_DETAIL_SEQUENTION_COMPOSITION_TASK_HPP
