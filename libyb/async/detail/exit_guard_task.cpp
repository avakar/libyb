#include "exit_guard_task.hpp"
#include "../cancel_exception.hpp"
#include "../task.hpp"
using namespace yb;
using namespace yb::detail;

exit_guard_task::exit_guard_task(cancel_level cancel_threshold)
	: m_threshold(cancel_threshold)
{
}

void exit_guard_task::cancel(cancel_level cl) throw()
{
	if (cl > m_cl)
		m_cl = cl;
}

task_result<void> exit_guard_task::cancel_and_wait() throw()
{
	m_cl = cl_kill;
	return task_result<void>();
}

void exit_guard_task::prepare_wait(task_wait_preparation_context & ctx)
{
	if (m_cl >= m_threshold)
		ctx.set_finished();
}

task<void> exit_guard_task::finish_wait(task_wait_finalization_context &) throw()
{
	return async::value();
}
