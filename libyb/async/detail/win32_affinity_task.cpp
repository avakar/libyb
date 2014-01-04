#include "win32_affinity_task.hpp"
#include "../cancel_exception.hpp"
#include "../../utils/except.hpp"
using namespace yb;

win32_affinity_task::win32_affinity_task()
	: m_cl(cl_none)
{
}

void win32_affinity_task::cancel(cancel_level cl) throw()
{
	if (m_cl < cl)
		m_cl = cl;
}

task<void> win32_affinity_task::cancel_and_wait() throw()
{
	return task<void>::from_exception(yb::make_exception_ptr(task_cancelled(cl_kill)));
}

void win32_affinity_task::prepare_wait(task_wait_preparation_context & ctx)
{
	ctx.set_finished();
}

task<void> win32_affinity_task::finish_wait(task_wait_finalization_context &) throw()
{
	return m_cl != cl_none? async::raise<void>(task_cancelled(m_cl)): async::value();
}
