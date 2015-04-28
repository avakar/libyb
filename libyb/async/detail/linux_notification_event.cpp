#include "notification_event.hpp"
#include "../../utils/detail/unix_system_error.hpp"
#include "linux_fdpoll_task.hpp"
#include <sys/eventfd.h>
#include <unistd.h>
using namespace yb;

notification_event::notification_event()
{
	m_linux_eventfd = eventfd(0, EFD_NONBLOCK);
	if (m_linux_eventfd == -1)
		throw yb::detail::unix_system_error(errno);
}

notification_event::~notification_event()
{
	if (m_linux_eventfd != -1)
		close(m_linux_eventfd);
}

notification_event::notification_event(notification_event && o)
	: m_linux_eventfd(o.m_linux_eventfd)
{
	o.m_linux_eventfd = -1;
}

notification_event & notification_event::operator=(notification_event && o)
{
	std::swap(m_linux_eventfd, o.m_linux_eventfd);
	return *this;
}

void notification_event::set()
{
	uint64_t val = 1;
	write(m_linux_eventfd, &val, sizeof val);
}

void notification_event::reset()
{
	uint64_t val;
	read(m_linux_eventfd, &val, sizeof val);
}

yb::task<void> notification_event::wait()
{
	return yb::make_linux_pollfd_task(m_linux_eventfd, POLLIN, [](yb::cancel_level cl) {
		return cl < yb::cl_abort;
	}).ignore_result();
}
