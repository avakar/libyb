#ifndef LIBYB_ASYNC_TASK_BASE_HPP
#define LIBYB_ASYNC_TASK_BASE_HPP

#include "cancel_level.hpp"

namespace yb {

template <typename R>
class task;

class task_wait_preparation_context;
class task_wait_finalization_context;

template <typename R>
class task_base
{
public:
	typedef R result_type;

	virtual ~task_base() {}

	// Cancels the task with `cancel_level_hard`
	// and synchronously waits for it to complete.
	virtual task<R> cancel_and_wait() throw() = 0;

	virtual void prepare_wait(task_wait_preparation_context & ctx, cancel_level cl) = 0;

	// An empty task indicates a stall.
	// A task-based task indicates a continuation.
	// A result task indicates a completion.
	virtual task<R> finish_wait(task_wait_finalization_context & ctx) throw() = 0;
};

template <typename R>
class future_base
	: public task_base<R>
{
public:
	virtual void cancel(cancel_level cl) throw() = 0;
};

} // namespace yb

#endif // LIBYB_ASYNC_TASK_BASE_HPP
