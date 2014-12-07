#ifndef LIBYB_ASYNC_DETAIL_RUNNER_IMPL_HPP
#define LIBYB_ASYNC_DETAIL_RUNNER_IMPL_HPP

#include "prepared_task.hpp"

namespace yb {

template <typename R>
task<R> runner::post(task<R> && t) throw()
{
	struct prepared_task_deleter
	{
		void operator()(detail::prepared_task_base * p) { p->release(); }
	};

	if (t.has_value() || t.has_exception())
		return std::move(t);

	try
	{
		std::unique_ptr<detail::prepared_task<R>, prepared_task_deleter> pt(new detail::prepared_task<R>(std::move(t)));
		this->submit(*pt);
		return task<R>::from_task(pt.release());
	}
	catch (...)
	{
		return async::raise<R>();
	}
}

template <typename R>
task<R> runner::try_run(task<R> && t) throw()
{
	if (t.has_value() || t.has_exception())
		return std::move(t);

	try
	{
		detail::prepared_task<R> pt(std::move(t));
		this->run_until(pt);
		return pt.get_result();
	}
	catch (...)
	{
		return task<R>::from_exception(std::current_exception());
	}
}

template <typename R>
R runner::run(task<R> && t)
{
	return this->try_run(std::move(t)).get();
}

template <typename R>
R runner::operator%(task<R> && t)
{
	return this->run(std::move(t));
}

} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_RUNNER_IMPL_HPP
