#include "parallel_composition_task.hpp"
#include "task_impl.hpp"
#include <stdexcept>
using namespace yb;
using namespace yb::detail;

parallel_composition_task::parallel_composition_task(task<void> && t, task<void> && u)
{
	m_tasks.resize(2);
	m_tasks.front().t = std::move(t);
	m_tasks.back().t = std::move(u);
}

void parallel_composition_task::cancel(cancel_level cl) throw()
{
	for (std::list<parallel_task>::iterator it = m_tasks.begin(); it != m_tasks.end(); ++it)
		it->t.cancel(cl);
}

task_result<void> parallel_composition_task::cancel_and_wait() throw()
{
	for (std::list<parallel_task>::iterator it = m_tasks.begin(); it != m_tasks.end(); ++it)
		it->t.cancel_and_wait(); // XXX: handle exc results
	m_tasks.clear();
	return task_result<void>();
}

void parallel_composition_task::prepare_wait(task_wait_preparation_context & ctx)
{
	for (std::list<parallel_task>::iterator it = m_tasks.begin(); it != m_tasks.end(); ++it)
	{
		task_wait_memento_builder mb(ctx);
		it->t.prepare_wait(ctx);
		it->m = mb.finish();
	}
}

task<void> parallel_composition_task::finish_wait(task_wait_finalization_context & ctx) throw()
{
	for (std::list<parallel_task>::iterator it = m_tasks.begin(); it != m_tasks.end(); )
	{
		if (ctx.contains(it->m))
		{
			it->t.finish_wait(ctx); // XXX: handle exc results
			if (it->t.has_result())
			{
				it = m_tasks.erase(it);
				continue;
			}
		}

		++it;
	}

	switch (m_tasks.size())
	{
	case 0:
		return async::value();
	case 1:
		return std::move(m_tasks.front().t);
	default:
		return nulltask;
	}
}

parallel_composition_task::parallel_task::parallel_task()
{
}

parallel_composition_task::parallel_task::parallel_task(parallel_task && o)
	: t(std::move(o.t))
{
}
