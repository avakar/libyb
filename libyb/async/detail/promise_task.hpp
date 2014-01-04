#ifndef LIBYB_ASYNC_DETAIL_PROMISE_TASK_HPP
#define LIBYB_ASYNC_DETAIL_PROMISE_TASK_HPP

#include "../../utils/except.hpp"

namespace yb {
namespace detail {

template <typename T>
class promise_task_impl
	: public task_base<T>
{
public:
	explicit promise_task_impl(shared_circular_buffer<task<T>, 1> * buffer)
		: m_buffer(buffer)
	{
		m_buffer->addref();
	}

	~promise_task_impl()
	{
		if (m_buffer)
			m_buffer->release();
	}

	void cancel(cancel_level cl) throw()
	{
		if (cl >= cl_abort && m_buffer && m_buffer->empty())
		{
			m_buffer->release();
			m_buffer = 0;
		}
	}

	task<T> cancel_and_wait() throw()
	{
		this->cancel(cl_kill);

		if (m_buffer)
			return std::move(m_buffer->front());
		else
			return task<T>::from_exception(yb::make_exception_ptr(task_cancelled()));
	}

	void prepare_wait(task_wait_preparation_context & ctx)
	{
		if (!m_buffer || !m_buffer->empty())
			ctx.set_finished();
	}

	task<T> finish_wait(task_wait_finalization_context &) throw()
	{
		if (m_buffer)
			return std::move(m_buffer->front());
		else
			return async::raise<T>(task_cancelled());
	}

private:
	shared_circular_buffer<task<T>, 1> * m_buffer;

	promise_task_impl(promise_task_impl const &);
	promise_task_impl & operator=(promise_task_impl const &);
};

} // namespace detail
} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_PROMISE_TASK_HPP
