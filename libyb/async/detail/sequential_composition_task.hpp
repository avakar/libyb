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

	task<R> cancel_and_wait() throw() override;
	void prepare_wait(task_wait_preparation_context & ctx) override;
	task<R> finish_wait(task_wait_finalization_context & ctx) throw() override;
	cancel_level cancel(cancel_level cl) throw() override;

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
	assert(!task.has_value() && !task.has_exception() && !task.empty());
	m_task = std::move(task);
}

template <typename R, typename S, typename F>
task<R> sequential_composition_task<R, S, F>::cancel_and_wait() throw()
{
	task<S> s = m_task.cancel_and_wait();
	try
	{
		task<R> r = m_next(std::move(s));
		if (r.has_task())
			return r.cancel_and_wait();

		assert(r.has_value() || r.has_exception());
		return std::move(r);
	}
	catch (...)
	{
		return task<R>::from_exception(std::current_exception());
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

	if (m_task.has_value() || m_task.has_exception())
	{
		try
		{
			return m_next(std::move(m_task));
		}
		catch (...)
		{
			return async::raise<R>();
		}
	}

	return nulltask;
}

template <typename R, typename S, typename F>
cancel_level sequential_composition_task<R, S, F>::cancel(cancel_level cl) throw()
{
	return m_task.cancel(cl);
}

}
}

#endif // LIBYB_ASYNC_DETAIL_SEQUENTION_COMPOSITION_TASK_HPP
