#ifndef LIBYB_ASYNC_DETAIL_RUNNER_IMPL_HPP
#define LIBYB_ASYNC_DETAIL_RUNNER_IMPL_HPP

#include "prepared_task_impl.hpp"

namespace yb {

template <typename R>
task<R> runner::post(task<R> && t)
{
	detail::prepared_task_guard<R> pt(new detail::prepared_task_impl<R>(std::move(t)));
	task<R> res(new detail::shadow_task<R>(pt.get()));
	this->submit(pt.get());
	return std::move(res);
}

template <typename R>
void runner::post_detached(task<R> && t)
{
	detail::prepared_task_guard<R> pt(new detail::prepared_task_impl<R>(std::move(t)));
	this->submit(pt.get());
}

template <typename R>
task_result<R> runner::try_run(task<R> && t)
{
	detail::prepared_task_guard<R> pt(new detail::prepared_task_impl<R>(std::move(t)));
	this->submit(pt.get());
	this->run_until(pt.get());
	return pt->fetch_result();
}

template <typename R>
R runner::run(task<R> && t)
{
	return this->try_run(std::move(t)).get();
}

} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_RUNNER_IMPL_HPP
