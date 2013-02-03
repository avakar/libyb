#ifndef LIBYB_ASYNC_DOUBLE_BUFFER_HPP
#define LIBYB_ASYNC_DOUBLE_BUFFER_HPP

#include "task.hpp"
#include <functional>

namespace yb {

template <typename R>
task<void> double_buffer(std::function<task<R>(size_t)> const & create_fn, std::function<void(size_t, R const &)> const & collect_fn, size_t count = 2);

} // namespace yb

#include "detail/double_buffer_task.hpp"

template <typename R>
yb::task<void> yb::double_buffer(std::function<task<R>(size_t)> const & create_fn, std::function<void(size_t, R const &)> const & collect_fn, size_t count)
{
	try
	{
		return task<void>(new detail::double_buffer_task<R>(count, create_fn, collect_fn));
	}
	catch (...)
	{
		return async::raise<void>();
	}
}

#endif // LIBYB_ASYNC_DOUBLE_BUFFER_HPP
