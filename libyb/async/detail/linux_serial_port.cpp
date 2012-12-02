#include "../serial_port.hpp"
#include "linux_fdpoll_task.hpp"
#include "../../utils/detail/scoped_unix_fd.hpp"
#include <stdexcept>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
using namespace yb;
using namespace yb::detail;

struct serial_port::impl
{
	scoped_unix_fd fd;
};

serial_port::serial_port()
	: m_pimpl(new impl())
{
}

serial_port::~serial_port()
{
}

task<void> serial_port::open(yb::string_ref const & name, settings const & s)
{
	return protect([&]() -> task<void> {
		int fd = ::open(std::string(name).c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
		if (fd == -1)
			return async::raise<void>(std::runtime_error("cannot open the port"));
		m_pimpl->fd.reset(fd);
		return async::value();
	});
}

task<void> serial_port::open(yb::string_ref const & name, int baudrate)
{
	settings s;
	s.baudrate = baudrate;
	return this->open(name, s);
}

void serial_port::close()
{
	m_pimpl->fd.reset();
}

task<size_t> serial_port::read(uint8_t * buffer, size_t size)
{
	return make_linux_pollfd_task(m_pimpl->fd.get(), POLLIN, [](cancel_level cl) {
		return cl < cl_abort;
	}).then([this, buffer, size](short revents) -> task<size_t> {
		if (!(revents & POLLIN))
			return async::raise<size_t>(std::runtime_error("poll failed"));
		return async::value((size_t)::read(m_pimpl->fd.get(), buffer, size));
	});
}

task<size_t> serial_port::write(uint8_t const * buffer, size_t size)
{
	return make_linux_pollfd_task(m_pimpl->fd.get(), POLLOUT, [](cancel_level cl) {
		return cl < cl_abort;
	}).then([this, buffer, size](short revents) -> task<size_t> {
		if (!(revents & POLLIN))
			return async::raise<size_t>(std::runtime_error("poll failed"));
		return async::value((size_t)::write(m_pimpl->fd.get(), buffer, size));
	});
}
