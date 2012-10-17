#ifndef LIBYB_ASYNC_DETAIL_WIN32_HANDLE_TASK_HPP
#define LIBYB_ASYNC_DETAIL_WIN32_HANDLE_TASK_HPP

#include "../task.hpp"
#include "win32_wait_context.hpp"
#include <windows.h>

namespace yb {

// The canceller must not throw an exception
template <typename Canceller>
class win32_handle_task
	: task_base<void>
{
public:
	win32_handle_task(HANDLE handle, Canceller const & canceller);
	win32_handle_task(HANDLE handle, Canceller && canceller);

	void cancel() throw();
	task_result<void> wait() throw();
	void prepare_wait(task_wait_preparation_context & ctx);
	task<void> finish_wait(task_wait_finalization_context & ctx) throw();

private:
	HANDLE m_handle;
	Canceller m_canceller;
};

template <typename Canceller>
task<void> make_win32_handle_task(HANDLE handle, Canceller && canceller) throw();

} // namespace yb

namespace yb {

template <typename Canceller>
win32_handle_task<Canceller>::win32_handle_task(HANDLE handle, Canceller const & canceller)
	: m_handle(handle), m_canceller(canceller)
{
}

template <typename Canceller>
win32_handle_task<Canceller>::win32_handle_task(HANDLE handle, Canceller && canceller)
	: m_handle(handle), m_canceller(std::move(canceller))
{
}

template <typename Canceller>
void win32_handle_task<Canceller>::prepare_wait(task_wait_preparation_context & ctx)
{
	if (m_handle)
		ctx.m_handles.push_back(m_handle);
	else
		++ctx.m_finished_tasks;
}

template <typename Canceller>
task<void> win32_handle_task<Canceller>::finish_wait(task_wait_finalization_context & ctx) throw()
{
	return m_handle? make_value_task(): make_value_task<void>(std::copy_exception(task_cancelled()));
}

template <typename Canceller>
task_result<void> win32_handle_task<Canceller>::wait() throw()
{
	if (m_handle)
	{
		WaitForSingleObject(m_handle, INFINITE);
		return task_result<void>();
	}
	else
	{
		return task_result<void>(std::copy_exception(task_cancelled()));
	}
}

template <typename Canceller>
void win32_handle_task<Canceller>::cancel() throw()
{
	if (!m_canceller())
		m_handle = 0;
}

template <typename Canceller>
task<void> make_win32_handle_task(HANDLE handle, Canceller && canceller) throw()
{
	try
	{
		return task<void>(
			new win32_handle_task<typename std::remove_reference<Canceller>::type>(handle, std::forward<Canceller>(canceller)));
	}
	catch (...)
	{
		if (canceller())
		{
			WaitForSingleObject(handle, INFINITE);
			return make_value_task();
		}

		return make_value_task<void>(std::current_exception());
	}
}

} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_WIN32_HANDLE_TASK_HPP
