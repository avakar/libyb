#include "parallel_composition_task.hpp"
#include "task_impl.hpp"
#include <stdexcept>
using namespace yb;
using namespace yb::detail;

parallel_composition_task::parallel_composition_task(task<void> && t, task<void> && u)
{
	m_compositor.add_task(std::move(t), std::move(u));
}

task<void> parallel_composition_task::cancel_and_wait() throw()
{
	m_compositor.cancel_and_wait([](task<void> const &) {});
	return async::value();
}

void parallel_composition_task::prepare_wait(task_wait_preparation_context & ctx, cancel_level cl)
{
	m_compositor.prepare_wait(ctx, cl);
}

task<void> parallel_composition_task::finish_wait(task_wait_finalization_context & ctx) throw()
{
	m_compositor.finish_wait(ctx, [](task<void> const &){});
	if (m_compositor.empty())
		return async::value();
	return nulltask;
}
