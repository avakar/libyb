#ifndef LIBYB_ASYNC_DETAIL_WIN32_HANDLE_TASK_HPP
#define LIBYB_ASYNC_DETAIL_WIN32_HANDLE_TASK_HPP

#include "../task.hpp"
#include "win32_affinity_task.hpp"
#include "win32_wait_context.hpp"
#include "../../utils/noncopyable.hpp"
#include "../../utils/except.hpp"
#include "../cancel_exception.hpp"
#include "win32_runner_registry.hpp"
#include <windows.h>

namespace yb {

// The canceller must not throw an exception
template <typename Canceller>
class win32_handle_task
	: public task_base<void>
	, handle_completion_sink
{
public:
	win32_handle_task(HANDLE handle, Canceller const & canceller);
	win32_handle_task(HANDLE handle, Canceller && canceller);

	task<void> start(runner_registry & rr, task_completion_sink<void> & sink) override;
	task<void> cancel(runner_registry * rr, cancel_level cl) throw() override;

private:
	void on_signaled(runner_registry & rr, HANDLE h) throw() override;

	task_completion_sink<void> * m_parent;
	HANDLE m_handle;
	Canceller m_canceller;
};

template <typename Canceller>
task<void> make_win32_handle_task(HANDLE handle, Canceller && canceller) throw();

} // namespace yb

namespace yb {

template <typename Canceller>
win32_handle_task<Canceller>::win32_handle_task(HANDLE handle, Canceller const & canceller)
	: m_parent(nullptr), m_handle(handle), m_canceller(canceller)
{
}

template <typename Canceller>
win32_handle_task<Canceller>::win32_handle_task(HANDLE handle, Canceller && canceller)
	: m_parent(nullptr), m_handle(handle), m_canceller(std::move(canceller))
{
}

template <typename Canceller>
task<void> win32_handle_task<Canceller>::start(runner_registry & rr, task_completion_sink<void> & sink)
{
	rr.add_handle(m_handle, *this);
	m_parent = &sink;
	return yb::nulltask;
}

template <typename Canceller>
void win32_handle_task<Canceller>::on_signaled(runner_registry & rr, HANDLE h) throw()
{
	assert(m_handle == h);
	m_parent->on_completion(rr, async::value());
}

template <typename Canceller>
task<void> win32_handle_task<Canceller>::cancel(runner_registry * rr, cancel_level cl) throw()
{
	if (cl >= cl_abort && !m_canceller(cl))
	{
		if (rr)
			rr->remove_handle(m_handle, *this);
		return async::raise<void>(task_cancelled());
	}

	if (cl == cl_kill)
	{
		WaitForSingleObject(m_handle, INFINITE);
		return async::value();
	}

	return yb::nulltask;
}

template <typename Canceller>
task<void> make_win32_handle_task(HANDLE handle, Canceller && canceller) throw()
{
	try
	{
		return task<void>::from_task(
			new win32_handle_task<typename std::remove_reference<Canceller>::type>(handle, std::forward<Canceller>(canceller)));
	}
	catch (...)
	{
		if (canceller(cl_kill))
		{
			WaitForSingleObject(handle, INFINITE);
			return async::value();
		}

		return async::raise<void>();
	}
}

namespace async {

task<void> fix_affinity();

}

} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_WIN32_HANDLE_TASK_HPP
