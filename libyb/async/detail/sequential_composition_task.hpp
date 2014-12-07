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
	: public task_base<R>, private task_completion_sink<S>
{
public:
	sequential_composition_task(task<S> && task, F next);

	task<R> start(runner_registry & rr, task_completion_sink<R> & sink) override;
	task<R> cancel(runner_registry * rr, cancel_level cl) throw() override;

private:
	void on_completion(runner_registry & rr, task<S> && r) override;

	task_completion_sink<R> * m_parent;
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

/*template <typename R, typename S, typename F>
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
}*/

template <typename R, typename S, typename F>
task<R> sequential_composition_task<R, S, F>::start(runner_registry & rr, task_completion_sink<R> & sink)
{
	assert(m_task.has_task());
	m_task.start(rr, *this);
	m_parent = &sink;
	return nulltask;
}

template <typename R, typename S, typename F>
void sequential_composition_task<R, S, F>::on_completion(runner_registry & rr, task<S> && r)
{
	if (m_task.replace(rr, *this, std::move(r)))
	{
		try
		{
			m_parent->on_completion(rr, m_next(std::move(m_task)));
		}
		catch (...)
		{
			m_parent->on_completion(rr, async::raise<R>());
		}
	}
}

template <typename R, typename S, typename F>
task<R> sequential_composition_task<R, S, F>::cancel(runner_registry * rr, cancel_level cl) throw()
{
	m_task.cancel(rr, cl);
	if (m_task.has_task())
		return yb::nulltask;

	try
	{
		return m_next(std::move(m_task));
	}
	catch (...)
	{
		return task<R>::from_exception(std::current_exception());
	}
}

}
}

#endif // LIBYB_ASYNC_DETAIL_SEQUENTION_COMPOSITION_TASK_HPP
