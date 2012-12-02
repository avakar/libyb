#ifndef LIBYB_ASYNC_DETAIL_LINUX_WAIT_CONTEXT_HPP
#define LIBYB_ASYNC_DETAIL_LINUX_WAIT_CONTEXT_HPP

#include "wait_context.hpp"
#include <vector>
#include <sys/poll.h>

namespace yb {

struct task_wait_poll_item
{
};

struct task_wait_preparation_context_impl
{
	std::vector<struct pollfd> m_pollfds;
	size_t m_finished_tasks;
};

} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_WIN32_WAIT_CONTEXT_HPP
