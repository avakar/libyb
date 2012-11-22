#include "../sync_runner.hpp"
#include "win32_wait_context.hpp"
using namespace yb;

void sync_runner::poll_one(task_wait_preparation_context & wait_ctx)
{
	task_wait_preparation_context_impl & wait_ctx_impl = *wait_ctx.get();

	wait_ctx.clear();
	m_parallel_tasks.prepare_wait(wait_ctx);

	if (wait_ctx_impl.m_finished_tasks)
	{
		task_wait_finalization_context finish_ctx;
		finish_ctx.finished_tasks = wait_ctx_impl.m_finished_tasks;
		m_parallel_tasks.finish_wait(finish_ctx);
	}
	else
	{
		assert(!wait_ctx_impl.m_handles.empty());

		DWORD dwRes = WaitForMultipleObjects(wait_ctx_impl.m_handles.size(), wait_ctx_impl.m_handles.data(), FALSE, INFINITE);
		assert(dwRes >= WAIT_OBJECT_0 && dwRes < WAIT_OBJECT_0 + wait_ctx_impl.m_handles.size());

		task_wait_finalization_context finish_ctx;
		finish_ctx.finished_tasks = false;
		finish_ctx.selected_poll_item = dwRes - WAIT_OBJECT_0;
		m_parallel_tasks.finish_wait(finish_ctx);
	}
}
