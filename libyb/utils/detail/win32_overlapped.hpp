#ifndef LIBYB_UTILS_DETAIL_WIN32_OVERLAPPED_HPP
#define LIBYB_UTILS_DETAIL_WIN32_OVERLAPPED_HPP

#include <stdexcept>
#include <windows.h>

namespace yb {
namespace detail {

struct win32_overlapped
{
	win32_overlapped()
		: o()
	{
		o.hEvent = CreateEvent(0, TRUE, FALSE, 0);
		if (!o.hEvent)
			throw std::runtime_error("couldn't create event for overlapped");
	}

	~win32_overlapped()
	{
		CloseHandle(o.hEvent);
	}

	OVERLAPPED o;
};

void cancel_win32_overlapped(HANDLE hFile, OVERLAPPED * o);
void cancel_win32_overlapped(HANDLE hFile, win32_overlapped & o);

} // namespace detail
} // namespace yb

#endif // LIBYB_UTILS_DETAIL_WIN32_OVERLAPPED_HPP
