#ifndef LIBYB_ASYNC_DETAIL_PARALLEL_COMPOSITION_TASK_HPP
#define LIBYB_ASYNC_DETAIL_PARALLEL_COMPOSITION_TASK_HPP

#include "parallel_compositor.hpp"
#include "../task_base.hpp"

namespace yb {
namespace detail {

class parallel_composition_task
	: public task_base<void>
{
public:
	parallel_composition_task(task<void> && t, task<void> && u);

	task<void> start(runner_registry & rr, task_completion_sink<void> & sink) override;
	task<void> cancel(runner_registry * rr, cancel_level cl) throw() override;

private:
	parallel_compositor m_compositor;
};

} // namespace detail
} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_PARALLEL_COMPOSITION_TASK_HPP
