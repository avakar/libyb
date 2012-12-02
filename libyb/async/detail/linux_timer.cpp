#include "../timer.hpp"
#include "linux_fdpoll_task.hpp"
#include "../../utils/detail/scoped_unix_fd.hpp"
#include <stdexcept>
#include <sys/timerfd.h>
#include <fcntl.h>
using namespace yb;
using namespace yb::detail;

struct timer::impl
{
	scoped_unix_fd fd;
};

timer::timer()
	: m_pimpl(new impl())
{
	m_pimpl->fd.reset(timerfd_create(CLOCK_MONOTONIC, 0));
	if (m_pimpl->fd.empty())
		throw std::runtime_error("cannot create timerfd");
	if (fcntl(m_pimpl->fd.get(), F_SETFL, O_NONBLOCK) == -1)
		throw std::runtime_error("cannot set O_NONBLOCK");
}

timer::~timer()
{
}

task<void> timer::wait_ms(int milliseconds)
{
	assert(milliseconds > 0);

	struct itimerspec ts = {};
	ts.it_value.tv_sec = milliseconds / 1000;
	ts.it_value.tv_nsec = (milliseconds % 1000) * 1000000;
	if (timerfd_settime(m_pimpl->fd.get(), 0, &ts, 0) != 0)
		return async::raise<void>(std::runtime_error("cannot set timerfd"));

	return make_linux_pollfd_task(m_pimpl->fd.get(), POLLIN, [this](cancel_level cl) {
		if (cl < cl_abort)
			return true;
		struct itimerspec ts = {};
		timerfd_settime(m_pimpl->fd.get(), 0, &ts, 0);
		return false;
	}).then([](short revents) {
		return revents & POLLIN? async::value(): async::raise<void>(std::runtime_error("timerfd poll error"));
	});
}
