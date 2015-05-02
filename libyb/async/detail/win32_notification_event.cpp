#include "notification_event.hpp"
#include "win32_handle_task.hpp"
#include "../../utils/detail/win32_system_error.hpp"
#include <windows.h>
using namespace yb;

notification_event::notification_event()
{
	m_win32_handle = ::CreateEvent(0, TRUE, FALSE, 0);
	if (!m_win32_handle)
		throw detail::win32_system_error(::GetLastError());
}

notification_event::~notification_event()
{
	if (m_win32_handle)
		::CloseHandle(m_win32_handle);
}

notification_event::notification_event(notification_event && o)
: m_win32_handle(o.m_win32_handle)
{
	o.m_win32_handle = 0;
}

notification_event & notification_event::operator=(notification_event && o)
{
	std::swap(m_win32_handle, o.m_win32_handle);
	return *this;
}

void notification_event::set()
{
	::SetEvent(m_win32_handle);
}

void notification_event::reset()
{
	::ResetEvent(m_win32_handle);
}

yb::task<void> notification_event::wait()
{
	return yb::make_win32_handle_task(m_win32_handle, [](yb::cancel_level cl) { return cl < yb::cl_abort; });
}
