#include "task_fwd.hpp"
#include "task_impl.hpp"
#include "parallel_composition_task.hpp"

yb::task<void> yb::operator|(task<void> && lhs, task<void> && rhs)
{
	try
	{
		return yb::task<void>(new detail::parallel_composition_task(std::move(lhs), std::move(rhs)));
	}
	catch (...)
	{
		return make_value_task<void>(std::current_exception());
	}
}

yb::task<void> & yb::operator|=(task<void> & lhs, task<void> && rhs)
{
	return lhs = std::move(lhs) | std::move(rhs);
}
