#ifndef LIBYB_ASYNC_RUNNER_HPP
#define LIBYB_ASYNC_RUNNER_HPP

#include "detail/prepared_task.hpp"
#include "task.hpp"
#include <memory>

namespace yb {

class runner
{
public:
	template <typename R>
	task<R> post(task<R> && t) throw();

	template <typename R>
	task_result<R> try_run(task<R> && t) throw();

	template <typename R>
	R run(task<R> && t) throw();

protected:
	virtual void submit(detail::prepared_task * pt) = 0;
	virtual void run_until(detail::prepared_task * pt) = 0;
};

} // namespace yb

#include "detail/runner_impl.hpp"

#endif // LIBYB_ASYNC_RUNNER_HPP
