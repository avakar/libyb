#ifndef LIBYB_ASYNC_DETAIL_WIN32_WAIT_CONTEXT_HPP
#define LIBYB_ASYNC_DETAIL_WIN32_WAIT_CONTEXT_HPP

#include <vector>
#include <windows.h>

namespace yb {

class task_wait_preparation_context
{
public:
	task_wait_preparation_context()
		: m_finished_tasks(0)
	{
	}

	void clear()
	{
		m_handles.clear();
		m_finished_tasks = 0;
	}

	std::vector<HANDLE> m_handles;
	size_t m_finished_tasks;
};

class task_wait_finalization_context
{
public:
	task_wait_finalization_context()
		: m_handle_finished_tasks(false), m_selected_handle(0)
	{
	}

	bool m_handle_finished_tasks;
	size_t m_selected_handle;
};

} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_WIN32_WAIT_CONTEXT_HPP
