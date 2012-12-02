#ifndef LIBYB_UTILS_DETAIL_SCOPED_UNIX_FD_HPP
#define LIBYB_UTILS_DETAIL_SCOPED_UNIX_FD_HPP

#include "../noncopyable.hpp"
#include <unistd.h>

namespace yb {
namespace detail {

class scoped_unix_fd
	: noncopyable
{
public:
	explicit scoped_unix_fd(int fd = -1)
		: m_fd(fd)
	{
	}

	scoped_unix_fd(scoped_unix_fd && o)
		: m_fd(o.m_fd)
	{
		o.m_fd = -1;
	}

	~scoped_unix_fd()
	{
		if (!this->empty())
			close(m_fd);
	}

	scoped_unix_fd & operator=(scoped_unix_fd o)
	{
		m_fd = o.m_fd;
		o.m_fd = -1;
		return *this;
	}

	bool empty() const
	{
		return m_fd == -1;
	}

	int get() const
	{
		return m_fd;
	}

	void reset(int fd = -1)
	{
		if (m_fd >= 0)
			close(m_fd);
		m_fd = fd;
	}

	int release()
	{
		int fd = m_fd;
		m_fd = -1;
		return fd;
	}

private:
	int m_fd;
};

} // namespace detail
} // namespace yb

#endif // LIBYB_UTILS_DETAIL_SCOPED_UNIX_FD_HPP
