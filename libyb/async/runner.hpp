#ifndef LIBYB_ASYNC_RUNNER_HPP
#define LIBYB_ASYNC_RUNNER_HPP

#include "detail/prepared_task.hpp"
#include "detail/runner_registry.hpp"
#include "task.hpp"
#include <memory>

namespace yb {

class runner
{
public:
	template <typename R>
	task<R> post(task<R> && t) throw();

	template <typename R>
	task<R> try_run(task<R> && t) throw();

	template <typename R>
	R run(task<R> && t);

	void run_forever() throw();

	template <typename R>
	R operator%(task<R> && t);

protected:
	virtual void submit(detail::prepared_task_base * pt) = 0;
	virtual void run_until(detail::prepared_task_base * pt) = 0;
};

} // namespace yb

#include "detail/runner_impl.hpp"

#endif // LIBYB_ASYNC_RUNNER_HPP
