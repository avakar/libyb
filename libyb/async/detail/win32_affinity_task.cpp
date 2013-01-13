#include "win32_affinity_task.hpp"
#include "../cancel_exception.hpp"
using namespace yb;

win32_affinity_task::win32_affinity_task()
{
}

void win32_affinity_task::cancel(cancel_level cl) throw()
{
	if (m_cl < cl)
		m_cl = cl;
}

task_result<void> win32_affinity_task::cancel_and_wait() throw()
{
	return task_result<void>(std::copy_exception(task_cancelled(cl_kill)));
}

void win32_affinity_task::prepare_wait(task_wait_preparation_context & ctx)
{
	ctx.set_finished();
}

task<void> win32_affinity_task::finish_wait(task_wait_finalization_context & ctx) throw()
{
	return m_cl != cl_none? async::raise<void>(task_cancelled(m_cl)): async::value();
}
