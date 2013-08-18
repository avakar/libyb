#ifndef LIBYB_UTILS_DETAIL_WIN32_MUTEX_HPP
#define LIBYB_UTILS_DETAIL_WIN32_MUTEX_HPP

#include <windows.h>

namespace yb {
namespace detail {

class win32_mutex
{
public:
	win32_mutex();
	~win32_mutex();

	void lock();
	void unlock();

private:
	CRITICAL_SECTION m_mutex;

	win32_mutex(win32_mutex const &);
	win32_mutex & operator=(win32_mutex const &);
};

class scoped_win32_lock
{
public:
	explicit scoped_win32_lock(win32_mutex & m)
		: m(m)
	{
		m.lock();
	}

	~scoped_win32_lock()
	{
		m.unlock();
	}

private:
	win32_mutex & m;

	scoped_win32_lock(scoped_win32_lock const &);
	scoped_win32_lock & operator=(scoped_win32_lock const &);
};

} // namespace detail
} // namespace yb

#endif // LIBYB_UTILS_DETAIL_WIN32_MUTEX_HPP
