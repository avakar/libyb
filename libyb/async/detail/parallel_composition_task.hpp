#ifndef LIBYB_ASYNC_DETAIL_PARALLEL_COMPOSITION_TASK_HPP
#define LIBYB_ASYNC_DETAIL_PARALLEL_COMPOSITION_TASK_HPP

#include "../task_base.hpp"
#include "wait_context.hpp"
#include <list>

namespace yb {
namespace detail {

class parallel_composition_task
	: public task_base<void>
{
public:
	parallel_composition_task(task<void> && t, task<void> && u);

	void cancel(cancel_level cl) throw();
	task<void> cancel_and_wait() throw();
	void prepare_wait(task_wait_preparation_context & ctx);
	task<void> finish_wait(task_wait_finalization_context & ctx) throw();

private:
	struct parallel_task
	{
		task<void> t;
		task_wait_memento m;

		parallel_task();
		parallel_task(parallel_task && o);
	};

	std::list<parallel_task> m_tasks;
};

} // namespace detail
} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_PARALLEL_COMPOSITION_TASK_HPP
