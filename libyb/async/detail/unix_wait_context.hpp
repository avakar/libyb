#ifndef LIBYB_ASYNC_DETAIL_UNIX_WAIT_CONTEXT_HPP
#define LIBYB_ASYNC_DETAIL_UNIX_WAIT_CONTEXT_HPP

#include "wait_context.hpp"
#include <vector>
#include <stddef.h>
#include <poll.h>

namespace yb {

struct task_wait_preparation_context_impl
{
	size_t m_finished_tasks;
	std::vector<struct pollfd> m_pollfds;
};

}

#endif // LIBYB_ASYNC_DETAIL_UNIX_WAIT_CONTEXT_HPP
