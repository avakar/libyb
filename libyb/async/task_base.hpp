#ifndef LIBYB_ASYNC_TASK_BASE_HPP
#define LIBYB_ASYNC_TASK_BASE_HPP

#include "cancel_level.hpp"

namespace yb {

template <typename R>
class task;

struct runner_registry;

template <typename R>
struct task_completion_sink
{
	virtual void on_completion(runner_registry & rr, task<R> && r) = 0;
};

template <typename R>
class task_base
{
public:
	typedef R result_type;

	virtual ~task_base() {}

	virtual task<R> start(runner_registry & rr, task_completion_sink<R> & sink) = 0;
	virtual task<R> cancel(runner_registry * rr, cancel_level cl) throw() = 0;
};

} // namespace yb

#endif // LIBYB_ASYNC_TASK_BASE_HPP
