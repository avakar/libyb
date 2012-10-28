#include "task_fwd.hpp"
#include "task_impl.hpp"
#include "parallel_composition_task.hpp"

yb::task<void> yb::operator|(task<void> && lhs, task<void> && rhs)
{
	if (!lhs.has_task())
		return std::move(rhs);

	if (!rhs.has_task())
		return std::move(lhs);

	try
	{
		return yb::task<void>(new detail::parallel_composition_task(std::move(lhs), std::move(rhs)));
	}
	catch (...)
	{
		return async::raise<void>();
	}
}

yb::task<void> & yb::operator|=(task<void> & lhs, task<void> && rhs)
{
	lhs = std::move(lhs) | std::move(rhs);
	return lhs;
}
