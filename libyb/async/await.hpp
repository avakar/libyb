#ifndef LIBYB_ASYNC_AWAIT_HPP
#define LIBYB_ASYNC_AWAIT_HPP

#include "sync_runner.hpp"

namespace yb {
namespace detail {

struct await_proxy
{
	template <typename T>
	T operator %(yb::task<T> && t)
	{
		yb::sync_runner r;
		return r.run(std::move(t));
	}
};

} // namespace detail
} // namespace yb

#define await ::yb::detail::await_proxy() %

#endif // LIBYB_ASYNC_AWAIT_HPP
