#ifndef LIBYB_ASYNC_DETAIL_PARALLEL_COMPOSITOR_HPP
#define LIBYB_ASYNC_DETAIL_PARALLEL_COMPOSITOR_HPP

#include "../task.hpp"
#include <list>

namespace yb {
namespace detail {

class parallel_compositor
{
public:
	parallel_compositor();
	~parallel_compositor();

	bool empty() const;

	void add_task(task<void> && t);
	void add_task(task<void> && t, task<void> && u);
	void append(parallel_compositor && o);
	task<void> pop();

	task<void> start(runner_registry & rr, task_completion_sink<void> & sink);
	task<void> cancel(runner_registry * rr, cancel_level cl) throw();

private:
	struct parallel_task_link
	{
		parallel_task_link * next;
		parallel_task_link * prev;
	};

	struct parallel_task;

	struct impl
		: parallel_task_link
	{
		task_completion_sink<void> * sink;
	};

	impl m_tasks;

	void append_task(parallel_task_link * pt);
	parallel_task_link * erase_task(parallel_task_link * pt);
};

} // namespace detail
} // namespace yb

#include "parallel_compositor_impl.hpp"

#endif // LIBYB_ASYNC_DETAIL_PARALLEL_COMPOSITOR_HPP
