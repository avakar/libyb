#ifndef LIBYB_ASYNC_DETAIL_CANCELLER_TASK_HPP
#define LIBYB_ASYNC_DETAIL_CANCELLER_TASK_HPP

#include "../task_base.hpp"
#include "../task.hpp"
#include <utility> // move

namespace yb {
namespace detail {

template <typename R, typename Canceller>
class canceller_task
	: public task_base<R>
{
public:
	canceller_task(task<R> && nested, Canceller const & canceller)
		: m_nested(std::move(nested)), m_canceller(canceller)
	{
	}

	void cancel() throw()
	{
		if (m_canceller())
			m_nested.cancel();
	}

	task_result<R> wait() throw()
	{
		return m_nested.wait();
	}

	void prepare_wait(task_wait_preparation_context & ctx)
	{
		m_nested.prepare_wait(ctx);
	}

	task<R> finish_wait(task_wait_finalization_context & ctx) throw()
	{
		m_nested.finish_wait(ctx);
	}

private:
	task<R> m_nested;
	Canceller m_canceller;
};

} // namespace detail
} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_CANCELLER_TASK_HPP
