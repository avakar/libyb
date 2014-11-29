#include "exit_guard_task.hpp"
#include "../cancel_exception.hpp"
#include "../task.hpp"
using namespace yb;
using namespace yb::detail;

exit_guard_task::exit_guard_task(cancel_level cancel_threshold)
	: m_cl(cl_none), m_threshold(cancel_threshold)
{
}

task<void> exit_guard_task::cancel_and_wait() throw()
{
	return task<void>::from_value();
}

void exit_guard_task::prepare_wait(task_wait_preparation_context & ctx)
{
	// XXX maybe we can pass cl here?
	if (m_cl >= m_threshold)
		ctx.set_finished();
}

task<void> exit_guard_task::finish_wait(task_wait_finalization_context &) throw()
{
	return async::value();
}

cancel_level exit_guard_task::cancel(cancel_level cl) throw()
{
	m_cl = cl;
	return cl;
}
