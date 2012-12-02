#include "../async_channel.hpp"
#include "linux_fdpoll_task.hpp"
#include "../../utils/detail/scoped_unix_fd.hpp"
#include <stdexcept>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/eventfd.h>
using namespace yb;
using namespace yb::detail;

struct async_channel_base::impl
{
	scoped_unix_fd fd;

	impl()
	{
		fd.reset(eventfd(0, 0));
		if (fd.empty())
			throw std::runtime_error("cannot create eventfd");
		if (fcntl(fd.get(), F_SETFL, O_NONBLOCK) == -1)
			throw std::runtime_error("cannot set O_NONBLOCK");
	}
};

async_channel_base::scoped_lock::scoped_lock(async_channel_base * ch)
	: m_ch(ch)
{
}

async_channel_base::scoped_lock::~scoped_lock()
{
}

async_channel_base::async_channel_base()
	: m_pimpl(new impl())
{
}

async_channel_base::~async_channel_base()
{
}

task<void> async_channel_base::wait()
{
	return make_linux_pollfd_task(m_pimpl->fd.get(), POLLIN, [](cancel_level cl) {
		return cl < cl_abort;
	}).ignore_result();
}

void async_channel_base::set()
{
	uint64_t val = 1;
	int r = write(m_pimpl->fd.get(), &val, sizeof val);
	assert(r != -1);
}

void async_channel_base::reset()
{
	uint64_t val;
	int r = read(m_pimpl->fd.get(), &val, sizeof val);
	assert(r != -1 || errno == EAGAIN);
}
