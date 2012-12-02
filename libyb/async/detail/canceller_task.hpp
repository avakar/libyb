#ifndef LIBYB_ASYNC_DETAIL_CANCELLER_TASK_HPP
#define LIBYB_ASYNC_DETAIL_CANCELLER_TASK_HPP

#include "../task_base.hpp"
#include "../task.hpp"
#include <utility> // forward

namespace yb {
namespace detail {

template <typename Nested, typename Canceller>
class canceller_task
	: public task_base<typename Nested::result_type>
{
public:
	typedef typename Nested::result_type result_type;

	canceller_task(Canceller const & canceller)
		: m_nested(), m_canceller(canceller)
	{
	}

	template <typename P1>
	canceller_task(Canceller const & canceller, P1 && p1)
		: m_nested(std::forward<P1>(p1)), m_canceller(canceller)
	{
	}

	template <typename P1, typename P2>
	canceller_task(Canceller const & canceller, P1 && p1, P2 && p2)
		: m_nested(std::forward<P1>(p1), std::forward<P2>(p2)), m_canceller(canceller)
	{
	}

	template <typename P1, typename P2, typename P3>
	canceller_task(Canceller const & canceller, P1 && p1, P2 && p2, P3 && p3)
		: m_nested(std::forward<P1>(p1), std::forward<P2>(p2), std::forward<P3>(p3)), m_canceller(canceller)
	{
	}

	template <typename P1, typename P2, typename P3, typename P4>
	canceller_task(Canceller const & canceller, P1 && p1, P2 && p2, P3 && p3, P4 && p4)
		: m_nested(std::forward<P1>(p1), std::forward<P2>(p2), std::forward<P3>(p3), std::forward<P4>(p4)), m_canceller(canceller)
	{
	}

	template <typename P1, typename P2, typename P3, typename P4, typename P5>
	canceller_task(Canceller const & canceller, P1 && p1, P2 && p2, P3 && p3, P4 && p4, P5 && p5)
		: m_nested(std::forward<P1>(p1), std::forward<P2>(p2), std::forward<P3>(p3), std::forward<P4>(p4), std::forward<P5>(p5)), m_canceller(canceller)
	{
	}

	void cancel(cancel_level cl) throw()
	{
		if (!m_canceller(cl))
			m_nested.cancel(cl);
	}

	task_result<result_type> cancel_and_wait() throw()
	{
		m_canceller(yb::cl_kill);
		return m_nested.cancel_and_wait();
	}

	void prepare_wait(task_wait_preparation_context & ctx)
	{
		m_nested.prepare_wait(ctx);
	}

	task<result_type> finish_wait(task_wait_finalization_context & ctx) throw()
	{
		return m_nested.finish_wait(ctx);
	}

private:
	Nested m_nested;
	Canceller m_canceller;
};

} // namespace detail
} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_CANCELLER_TASK_HPP
