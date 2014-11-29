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

	task<void> cancel_and_wait() throw() override;
	void prepare_wait(task_wait_preparation_context & ctx) override;
	task<void> finish_wait(task_wait_finalization_context & ctx) throw() override;
	cancel_level cancel(cancel_level cl) throw() override;

private:
	parallel_compositor m_compositor;
};

} // namespace detail
} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_PARALLEL_COMPOSITION_TASK_HPP
