#include "../file.hpp"
#include "../../utils/detail/unix_system_error.hpp"
#include "linux_fdpoll_task.hpp"
#include "unix_file.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
using namespace yb;

namespace {

struct reader_t
{
public:
	reader_t(int fd, buffer && buf)
		: m_fd(fd), m_buf(std::move(buf))
	{
	}

	task<buffer_view> operator()()
	{
		for (;;)
		{
			ssize_t r = ::read(m_fd, m_buf.data(), m_buf.size());
			if (r == -1)
			{
				int e = errno;

				if (e == EINTR)
					continue;

				if (e == EAGAIN || e == EWOULDBLOCK)
				{
					return make_linux_pollfd_task(m_fd, POLLOUT, [](cancel_level cl) {
						return cl < cl_abort;
					}).ignore_result().then(std::move(*this));
				}

				return async::raise<buffer_view>(detail::unix_system_error(e));
			}

			return async::value(buffer_view(std::move(m_buf), r));
		}
	}

private:
	int m_fd;
	buffer && m_buf;
};

task<buffer_view> fetch_and_read(int fd, buffer_policy policy, size_t max_size)
{
	return policy.fetch(1, max_size).then([fd, max_size](buffer buf) {
		return reader_t(fd, std::move(buf))();
	});
}

}

task<buffer_view> yb::detail::read_linux_fd(int fd, buffer_policy policy, size_t max_size)
{
	struct pollfd pf = {};
	pf.fd = fd;
	pf.events = POLLIN;

	for (;;)
	{
		int r = ::poll(&pf, 1, 0);
		if (r == -1)
		{
			int e = errno;
			if (e == EINTR)
				continue;
			return async::raise<buffer_view>(unix_system_error(e));
		}

		struct cont_t
		{
		public:
			cont_t(int fd, buffer_policy policy, size_t max_size)
				: m_fd(fd), m_policy(std::move(policy)), m_max_size(max_size)
			{
			}

			task<buffer_view> operator()(short revents)
			{
				(void)revents;
				return fetch_and_read(m_fd, std::move(m_policy), m_max_size);
			}

		private:
			int m_fd;
			buffer_policy m_policy;
			size_t m_max_size;
		};

		if (r == 0)
		{
			return make_linux_pollfd_task(fd, POLLIN, [](cancel_level cl) {
				return cl < cl_abort;
			}).then(cont_t(fd, std::move(policy), max_size));
		}

		return fetch_and_read(fd, std::move(policy), max_size);
	}
}

task<size_t> yb::detail::write_linux_fd(int fd, buffer_ref buf)
{
	return yb::loop2<short>([fd, buf](task<short> & r, cancel_level cl) -> task<size_t> {
		while (cl < cl_abort)
		{
			ssize_t rr = ::write(fd, buf.data(), buf.size());
			if (rr >= 0)
				return async::value((size_t)rr);

			int e = errno;
			if (e == EINTR)
				continue;

			if (e == EAGAIN || e == EWOULDBLOCK)
			{
				r = make_linux_pollfd_task(fd, POLLOUT, [](cancel_level cl) { return cl < cl_abort; });
				return yb::nulltask;
			}

			return async::raise<size_t>(detail::unix_system_error(e));
		}

		return async::raise<size_t>(task_cancelled(cl));
	});
}

task<size_t> yb::detail::send_linux_fd(int fd, buffer_ref buf, int flags)
{
	return yb::loop2<short>([fd, buf, flags](task<short> & r, cancel_level cl) -> task<size_t> {
		while (cl < cl_abort)
		{
			ssize_t rr = ::send(fd, buf.data(), buf.size(), flags);
			if (rr >= 0)
				return async::value((size_t)rr);

			int e = errno;
			if (e == EINTR)
				continue;

			if (e == EAGAIN || e == EWOULDBLOCK)
			{
				r = make_linux_pollfd_task(fd, POLLOUT, [](cancel_level cl) { return cl < cl_abort; });
				return yb::nulltask;
			}

			return async::raise<size_t>(detail::unix_system_error(e));
		}

		return async::raise<size_t>(task_cancelled(cl));
	});
}

struct file::impl
{
	int fd;
};

file::file(yb::string_ref const & fname)
{
	std::string zfname = fname;

	std::unique_ptr<impl> pimpl(new impl());
	pimpl->fd = open(zfname.c_str(), O_CLOEXEC | O_NONBLOCK | O_RDWR);
	if (pimpl->fd == -1)
		throw detail::unix_system_error(errno);
	m_pimpl = pimpl.release();
}

file::~file()
{
	this->clear();
}

void file::clear()
{
	if (m_pimpl)
	{
		close(m_pimpl->fd);
		delete m_pimpl;
		m_pimpl = 0;
	}
}

file::file_size_t file::size() const
{
	struct stat st;
	if (fstat(m_pimpl->fd, &st) == -1)
		throw detail::unix_system_error(errno);
	return st.st_size;
}

task<buffer_view> file::read(buffer_policy policy, size_t max_size)
{
	return detail::read_linux_fd(m_pimpl->fd, std::move(policy), max_size);
}

task<size_t> file::write(buffer_ref buf)
{
	return detail::write_linux_fd(m_pimpl->fd, buf);
}
