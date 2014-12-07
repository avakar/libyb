#include "win32_affinity_task.hpp"
#include "win32_runner_registry.hpp"
#include "../cancel_exception.hpp"
#include "../../utils/except.hpp"
using namespace yb;

task<void> win32_affinity_task::start(runner_registry & rr, task_completion_sink<void> & sink)
{
	(void)rr;
	(void)sink;
	return async::value();
}

task<void> win32_affinity_task::cancel(runner_registry * rr, cancel_level cl) throw()
{
	return cl != cl_none? async::raise<void>(task_cancelled(cl)): yb::nulltask;
}
