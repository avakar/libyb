#include "semaphore.hpp"
#include "win32_handle_task.hpp"
#include "../../utils/detail/win32_system_error.hpp"
#include <windows.h>
using namespace yb;

semaphore::semaphore()
{
	m_win32_handle = ::CreateEvent(0, FALSE, FALSE, 0);
	if (!m_win32_handle)
		throw detail::win32_system_error(::GetLastError());
}

semaphore::~semaphore()
{
	if (m_win32_handle)
		::CloseHandle(m_win32_handle);
}

semaphore::semaphore(semaphore && o)
	: m_win32_handle(o.m_win32_handle)
{
	o.m_win32_handle = 0;
}

semaphore & semaphore::operator=(semaphore && o)
{
	std::swap(m_win32_handle, o.m_win32_handle);
	return *this;
}

void semaphore::set()
{
	::SetEvent(m_win32_handle);
}

yb::task<void> semaphore::wait()
{
	return yb::make_win32_handle_task(m_win32_handle, [](yb::cancel_level cl) { return cl >= yb::cl_abort; });
}
