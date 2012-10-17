#ifndef LIBYB_ASYNC_TASK_BASE_HPP
#define LIBYB_ASYNC_TASK_BASE_HPP

#include "task_result.hpp"
#include "detail/task_fwd.hpp"
#include <memory> // unique_ptr

namespace yb {

class task_wait_preparation_context;
class task_wait_finalization_context;

struct task_base_common
{
	virtual ~task_base_common() throw();
	virtual void cancel() throw() = 0;
};

template <typename R>
class task_base
	: public task_base_common
{
public:
	virtual task_result<R> wait() throw() = 0;

	virtual void prepare_wait(task_wait_preparation_context & ctx) = 0;

	// An empty task indicates a stall.
	// A task-based task indicates a continuation.
	// A result task indicates a completion.
	virtual task<R> finish_wait(task_wait_finalization_context & ctx) throw() = 0;
};

} // namespace yb

namespace yb {

inline task_base_common::~task_base_common() throw()
{
}

} // namespace yb

#endif // LIBYB_ASYNC_TASK_BASE_HPP
