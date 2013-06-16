#ifndef LIBYB_UTILS_DETAIL_SCOPED_WIN32_HANDLE_HPP
#define LIBYB_UTILS_DETAIL_SCOPED_WIN32_HANDLE_HPP

#include "../noncopyable.hpp"
#include <windows.h>

namespace yb {
namespace detail {

class scoped_win32_handle
	: noncopyable
{
public:
	explicit scoped_win32_handle(HANDLE handle = INVALID_HANDLE_VALUE)
		: m_handle(handle)
	{
	}

	scoped_win32_handle(scoped_win32_handle && o)
		: m_handle(o.m_handle)
	{
		o.m_handle = INVALID_HANDLE_VALUE;
	}

	~scoped_win32_handle()
	{
		if (!this->empty())
			CloseHandle(m_handle);
	}

	scoped_win32_handle & operator=(scoped_win32_handle o)
	{
		m_handle = o.m_handle;
		o.m_handle = INVALID_HANDLE_VALUE;
		return *this;
	}

	bool empty() const
	{
		return m_handle == INVALID_HANDLE_VALUE;
	}

	HANDLE get() const
	{
		return m_handle;
	}

	HANDLE release()
	{
		HANDLE handle = m_handle;
		m_handle = INVALID_HANDLE_VALUE;
		return handle;
	}

private:
	HANDLE m_handle;
};

} // namespace detail
} // namespace yb

#endif // LIBYB_UTILS_DETAIL_SCOPED_WIN32_HANDLE_HPP
