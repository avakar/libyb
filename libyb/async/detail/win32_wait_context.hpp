#ifndef LIBYB_ASYNC_DETAIL_WIN32_WAIT_CONTEXT_HPP
#define LIBYB_ASYNC_DETAIL_WIN32_WAIT_CONTEXT_HPP

#include "wait_context.hpp"
#include <vector>
#include <windows.h>

namespace yb {

struct task_wait_poll_item
{
	HANDLE handle;
};

struct task_wait_preparation_context_impl
{
	std::vector<HANDLE> m_handles;
	size_t m_finished_tasks;
};

} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_WIN32_WAIT_CONTEXT_HPP
