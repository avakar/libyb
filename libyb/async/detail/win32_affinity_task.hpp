#ifndef LIBYB_ASYNC_WIN32_AFFINITY_TASK_HPP
#define LIBYB_ASYNC_WIN32_AFFINITY_TASK_HPP

#include "../task.hpp"

namespace yb {

class win32_affinity_task
	: public task_base<void>, noncopyable
{
public:
	task<void> start(runner_registry & rr, task_completion_sink<void> & sink) override;
	task<void> cancel(runner_registry * rr, cancel_level cl) throw() override;
};

} // namespace yb

#endif // LIBYB_ASYNC_WIN32_AFFINITY_TASK_HPP
