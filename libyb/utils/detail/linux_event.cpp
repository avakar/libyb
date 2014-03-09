#include "linux_event.hpp"
#include "unix_system_error.hpp"
#include <sys/eventfd.h>
#include <unistd.h>
#include <stdexcept>
using namespace yb::detail;

linux_event::linux_event()
{
	m_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
	if (m_fd == -1)
		throw unix_system_error(errno);
}

linux_event::~linux_event()
{
	::close(m_fd);
}

void linux_event::set()
{
	uint64_t val = 1;
	::write(m_fd, &val, sizeof val);
}

void linux_event::reset()
{
	uint64_t val;
	::read(m_fd, &val, sizeof val);
}

void linux_event::wait() const
{
	struct pollfd pf = this->get_poll();
	::poll(&pf, 1, -1);
}

struct pollfd linux_event::get_poll() const
{
	struct pollfd pf = {};
	pf.fd = m_fd;
	pf.events = POLLIN;
	return pf;
}
