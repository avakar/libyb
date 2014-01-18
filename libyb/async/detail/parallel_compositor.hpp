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

	bool empty() const;
	size_t size() const;

	void add_task(task<void> && t);
	void add_task(task<void> && t, task<void> && u);
	void append(parallel_compositor && o);
	task<void> pop();

	template <typename F>
	void cancel_and_wait(F f) throw();

	void prepare_wait(task_wait_preparation_context & ctx, cancel_level cl);

	template <typename F>
	void finish_wait(task_wait_finalization_context & ctx, F f) throw();

private:
	struct parallel_task
	{
		task<void> t;
		task_wait_memento m;
	};

	std::list<parallel_task> m_tasks;
	std::size_t m_task_count;
};

} // namespace detail
} // namespace yb

#include "parallel_compositor_impl.hpp"

#endif // LIBYB_ASYNC_DETAIL_PARALLEL_COMPOSITOR_HPP
