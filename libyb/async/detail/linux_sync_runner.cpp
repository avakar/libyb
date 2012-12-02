#include "../sync_runner.hpp"
#include "linux_wait_context.hpp"
using namespace yb;

void sync_runner::poll_one(task_wait_preparation_context & wait_ctx)
{
	task_wait_preparation_context_impl & wait_ctx_impl = *wait_ctx.get();

	wait_ctx.clear();
	m_parallel_tasks.prepare_wait(wait_ctx);

	if (wait_ctx_impl.m_finished_tasks)
	{
		task_wait_finalization_context finish_ctx;
		finish_ctx.prep_ctx = &wait_ctx;
		finish_ctx.finished_tasks = wait_ctx_impl.m_finished_tasks;
		m_parallel_tasks.finish_wait(finish_ctx);
	}
	else
	{
		int r = poll(wait_ctx_impl.m_pollfds.data(), wait_ctx_impl.m_pollfds.size(), -1);
		assert(r > 0);

		for (size_t i = 0; r != 0 && i < wait_ctx_impl.m_pollfds.size(); ++i)
		{
			if (wait_ctx_impl.m_pollfds[i].revents)
			{
				task_wait_finalization_context finish_ctx;
				finish_ctx.prep_ctx = &wait_ctx;
				finish_ctx.finished_tasks = false;
				finish_ctx.selected_poll_item = i;
				m_parallel_tasks.finish_wait(finish_ctx);

				--r;
			}
		}
	}
}
