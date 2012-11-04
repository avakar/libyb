#ifndef LIBYB_ASYNC_FUTURE_HPP
#define LIBYB_ASYNC_FUTURE_HPP

#include "detail/circular_buffer.hpp"
#include "task.hpp"
#include <stdexcept>

namespace yb {

namespace detail {

template <typename T>
class promise_task_impl
	: public task_base<T>
{
public:
	explicit promise_task_impl(shared_circular_buffer<task_result<T>, 1> * buffer)
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

	task_result<T> cancel_and_wait() throw()
	{
		this->cancel(cl_kill);

		if (m_buffer)
			return task_result<T>(m_buffer->front());
		else
			return task_result<T>(std::copy_exception(task_cancelled()));
	}

	void prepare_wait(task_wait_preparation_context & ctx)
	{
		if (!m_buffer || !m_buffer->empty())
			ctx.set_finished();
	}

	task<T> finish_wait(task_wait_finalization_context &) throw()
	{
		if (m_buffer)
			return async::result(m_buffer->front());
		else
			return async::raise<T>(task_cancelled());
	}

private:
	shared_circular_buffer<task_result<T>, 1> * m_buffer;

	promise_task_impl(promise_task_impl const &);
	promise_task_impl & operator=(promise_task_impl const &);
};

template <typename T>
class promise_base
{
public:
	promise_base()
		: m_buffer(new shared_circular_buffer<task_result<T>, 1>())
	{
	}

	promise_base(promise_base const & o)
		: m_buffer(o.m_buffer)
	{
		m_buffer->addref();
	}

	~promise_base()
	{
		m_buffer->release();
	}

	promise_base & operator=(promise_base const & o)
	{
		o.m_buffer->addref();
		m_buffer->release();
		m_buffer = o.m_buffer;
		return *this;
	}

	task<T> wait_for() const
	{
		return protect([this] {
			return task<T>(new promise_task_impl<T>(m_buffer));
		});
	}

	friend task<T> wait_for(promise_base const & p)
	{
		return p.wait_for();
	}

protected:
	shared_circular_buffer<task_result<T>, 1> * m_buffer;
};

} // namespace detail

template <typename T>
class promise
	: public detail::promise_base<T>
{
public:
	void set_value(T && t) const
	{
		m_buffer->push_back(task_result<T>(std::move(t)));
	}

	void set_value(T const & t) const
	{
		m_buffer->push_back(task_result<T>(t));
	}
};

template <>
class promise<void>
	: public detail::promise_base<void>
{
public:
	void set_value() const
	{
		m_buffer->push_back(task_result<void>());
	}
};

} // namespace yb

#endif // LIBYB_ASYNC_FUTURE_HPP
