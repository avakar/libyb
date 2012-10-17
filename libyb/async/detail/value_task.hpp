#ifndef LIBYB_ASYNC_DETAIL_VALUE_TASK_HPP
#define LIBYB_ASYNC_DETAIL_VALUE_TASK_HPP

#include "../task_base.hpp"
#include "../task_result.hpp"
#include <utility> // move

namespace yb {
namespace detail {

template <typename R>
class value_task
	: task_base<R>
{
public:
	value_task(task_result<R> v)
		: task_base<R>(st_done), m_value(std::move(v))
	{
	}

	task_result<R> get_result() throw()
	{
		return std::move(m_value);
	}

private:
	task_result<R> m_value;
};

} // namespace detail
} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_VALUE_TASK_HPP
