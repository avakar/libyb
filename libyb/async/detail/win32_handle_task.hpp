#ifndef LIBYB_ASYNC_DETAIL_WIN32_HANDLE_TASK_HPP
#define LIBYB_ASYNC_DETAIL_WIN32_HANDLE_TASK_HPP

#include "../task.hpp"
#include "win32_affinity_task.hpp"
#include "win32_wait_context.hpp"
#include "../../utils/noncopyable.hpp"
#include "../../utils/except.hpp"
#include "../cancel_exception.hpp"
#include <windows.h>

namespace yb {

// The canceller must not throw an exception
template <typename Canceller>
class win32_handle_task
	: public task_base<void>, noncopyable
{
public:
	win32_handle_task(HANDLE handle, Canceller const & canceller);
	win32_handle_task(HANDLE handle, Canceller && canceller);

	task<void> cancel_and_wait() throw() override;
	void prepare_wait(task_wait_preparation_context & ctx) override;
	task<void> finish_wait(task_wait_finalization_context & ctx) throw() override;
	cancel_level cancel(cancel_level cl) throw() override;

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
		ctx.get()->m_handles.push_back(m_handle);
	else
		++ctx.get()->m_finished_tasks;
}

template <typename Canceller>
task<void> win32_handle_task<Canceller>::finish_wait(task_wait_finalization_context &) throw()
{
	return m_handle? async::value(): async::raise<void>(task_cancelled());
}

template <typename Canceller>
cancel_level win32_handle_task<Canceller>::cancel(cancel_level cl) throw()
{
	if (cl < cl_abort)
		return cl;

	if (!m_canceller(cl))
		m_handle = 0;

	return cl_kill;
}

template <typename Canceller>
task<void> win32_handle_task<Canceller>::cancel_and_wait() throw()
{
	if (m_handle && !m_canceller(cl_kill))
		m_handle = 0;

	if (m_handle)
	{
		WaitForSingleObject(m_handle, INFINITE);
		return task<void>::from_value();
	}
	else
	{
		return task<void>::from_exception(yb::make_exception_ptr(task_cancelled()));
	}
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
