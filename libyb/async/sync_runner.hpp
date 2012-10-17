#ifndef LIBYB_ASYNC_SYNC_RUNNER_HPP
#define LIBYB_ASYNC_SYNC_RUNNER_HPP

#include "task.hpp"
#include "detail/win32_wait_context.hpp"
#include <utility> //move

namespace yb {

template <typename T>
task_result<T> try_run(task<T> && t)
{
	task_wait_preparation_context wait_ctx;

	assert(!t.empty());
	while (!t.has_result())
	{
		wait_ctx.clear();
		t.prepare_wait(wait_ctx);

		if (wait_ctx.m_finished_tasks)
		{
			task_wait_finalization_context finish_ctx;
			finish_ctx.m_handle_finished_tasks = true;
			t.finish_wait(finish_ctx);
		}
		else
		{
			assert(!wait_ctx.m_handles.empty());

			DWORD dwRes = WaitForMultipleObjects(wait_ctx.m_handles.size(), wait_ctx.m_handles.data(), FALSE, INFINITE);
			assert(dwRes >= WAIT_OBJECT_0 && dwRes < WAIT_OBJECT_0 + wait_ctx.m_handles.size());

			task_wait_finalization_context finish_ctx;
			finish_ctx.m_selected_handle = dwRes - WAIT_OBJECT_0;
			t.finish_wait(finish_ctx);
		}
	}

	return t.get_result();
}

template <typename T>
T run(task<T> && t)
{
	return try_run(std::move(t)).get();
}

} // namespace yb

#endif // LIBYB_ASYNC_SYNC_RUNNER_HPP
