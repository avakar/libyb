#ifndef LIBYB_ASYNC_DETAIL_EXIT_GUARD_TASK_HPP
#define LIBYB_ASYNC_DETAIL_EXIT_GUARD_TASK_HPP

#include "../task_base.hpp"

namespace yb {
namespace detail {

class exit_guard_task
	: public task_base<void>
{
public:
	explicit exit_guard_task(cancel_level cancel_threshold);

	task<void> cancel_and_wait() throw() override;
	void prepare_wait(task_wait_preparation_context & ctx) override;
	task<void> finish_wait(task_wait_finalization_context & ctx) throw() override;
	cancel_level cancel(cancel_level cl) throw() override;

private:
	cancel_level m_cl;
	cancel_level m_threshold;
};

} // namespace detail
} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_EXIT_GUARD_TASK_HPP
