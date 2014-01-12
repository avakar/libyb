#ifndef LIBYB_ASYNC_WIN32_AFFINITY_TASK_HPP
#define LIBYB_ASYNC_WIN32_AFFINITY_TASK_HPP

#include "../task.hpp"

namespace yb {

class win32_affinity_task
	: public task_base<void>, noncopyable
{
public:
	win32_affinity_task();

	task<void> cancel_and_wait() throw() override;
	void prepare_wait(task_wait_preparation_context & ctx, cancel_level cl) override;
	task<void> finish_wait(task_wait_finalization_context & ctx) throw() override;

private:
	cancel_level m_cl;
};

} // namespace yb

#endif // LIBYB_ASYNC_WIN32_AFFINITY_TASK_HPP
