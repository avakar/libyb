#include "parallel_composition_task.hpp"
#include "task_impl.hpp"
#include <stdexcept>
using namespace yb;
using namespace yb::detail;

parallel_composition_task::parallel_composition_task(task<void> && t, task<void> && u)
{
	m_compositor.add_task(std::move(t), std::move(u));
}

task<void> parallel_composition_task::start(runner_registry & rr, task_completion_sink<void> & sink)
{
	m_compositor.start(rr, sink);
	return m_compositor.empty()? async::value(): yb::nulltask;
}

task<void> parallel_composition_task::cancel(runner_registry * rr, cancel_level cl) throw()
{
	m_compositor.cancel(rr, cl);
	return m_compositor.empty()? async::value(): yb::nulltask;
}
