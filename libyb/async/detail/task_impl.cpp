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
		return yb::task<void>::from_task(new detail::parallel_composition_task(std::move(lhs), std::move(rhs)));
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

yb::task<void> yb::detail::task_value_traits<void>::from_value()
{
	task<void> res;
	res.m_kind = task<void>::k_value;
	return std::move(res);
}

void yb::detail::task_value_traits<void>::get_value()
{
}

void yb::detail::task_value_traits<void>::move_from(task<void> & other)
{
}

void yb::detail::task_value_traits<void>::clear_value()
{
}

yb::task<void> yb::async::value()
{
	return task<void>::from_value();
}

template <>
yb::task<void> yb::task<void>::finish_on(cancel_level cl, cancel_level abort_cl)
{
	if (m_kind == k_task)
	{
		try
		{
			return task<void>::from_task(new detail::cancel_level_upgrade_task<void>(std::move(*this), cl, abort_cl, true));
		}
		catch (...)
		{
			return this->cancel_and_wait();
		}
	}

	return std::move(*this);
}

template <>
yb::task<void> yb::task<void>::ignore_result()
{
	return std::move(*this);
}

namespace {

class infinite_loop_task
	: public yb::task_base<void>
{
public:
	yb::task<void> start(yb::runner_registry & rr, yb::task_completion_sink<void> & sink) override
	{
		return yb::nulltask;
	}

	yb::task<void> cancel(yb::runner_registry * rr, yb::cancel_level cl) throw() override
	{
		return cl >= yb::cl_quit? yb::async::value(): yb::nulltask;
	}
};

}

yb::task<void> yb::async::infinite_loop()
{
	return yb::protect([]() {
		return task<void>::from_task(new infinite_loop_task());
	});
}
