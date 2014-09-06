#ifndef LIBYB_ASYNC_DETAIL_RUNNER_IMPL_HPP
#define LIBYB_ASYNC_DETAIL_RUNNER_IMPL_HPP

#include "prepared_task_impl.hpp"

namespace yb {

template <typename R>
task<R> runner::post(task<R> && t) throw()
{
	if (t.has_value() || t.has_exception())
		return std::move(t);

	try
	{
		detail::prepared_task_guard<R> pt(new detail::prepared_task_impl<R>(std::move(t)));
		task<R> res(task<R>::from_future(new detail::shadow_task<R>(pt.get())));
		this->submit(pt.get());
		return std::move(res);
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
		detail::prepared_task_guard<R> pt(new detail::prepared_task_impl<R>(std::move(t)));
		this->submit(pt.get());
		this->run_until(pt.get());
		return pt->fetch_result();
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

inline void runner::run_forever() throw()
{
	this->run_until(0);
}

} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_RUNNER_IMPL_HPP
