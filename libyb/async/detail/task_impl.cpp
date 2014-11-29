#include "task_fwd.hpp"
#include "task_impl.hpp"
#include "parallel_composition_task.hpp"
#include "exit_guard_task.hpp"

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

yb::task<void> yb::async::exit_guard(cancel_level cancel_threshold)
{
	try
	{
		return task<void>::from_task(new detail::exit_guard_task(cancel_threshold));
	}
	catch (...)
	{
		return async::raise<void>();
	}
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
	infinite_loop_task()
		: m_cl(yb::cl_none)
	{
	}

	yb::task<void> cancel_and_wait() throw() override
	{
		return yb::async::value();
	}

	void prepare_wait(yb::task_wait_preparation_context & ctx) override
	{
		(void)ctx;
		if (m_cl >= yb::cl_quit)
			ctx.set_finished();
	}

	yb::task<void> finish_wait(yb::task_wait_finalization_context & ctx) throw() override
	{
		return yb::async::value();
	}

	yb::cancel_level cancel(yb::cancel_level cl) throw() override
	{
		m_cl = cl;
		return cl;
	}

private:
	yb::cancel_level m_cl;
};

}

yb::task<void> yb::async::infinite_loop()
{
	return yb::protect([]() {
		return task<void>::from_task(new infinite_loop_task());
	});
}
