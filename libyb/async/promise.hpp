#ifndef LIBYB_ASYNC_FUTURE_HPP
#define LIBYB_ASYNC_FUTURE_HPP

#include "detail/circular_buffer.hpp"
#include "detail/promise_task.hpp"
#include "detail/canceller_task.hpp"
#include "task.hpp"
#include <stdexcept>

namespace yb {
namespace detail {

template <typename T>
class promise_base
{
public:
	promise_base()
		: m_buffer(new shared_circular_buffer<task<T>, 1>())
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
			return task<T>::from_task(new promise_task_impl<T>(m_buffer));
		});
	}

	template <typename Canceller>
	task<T> wait_for(Canceller const & canceller) const
	{
		return protect([this, &canceller] {
			return task<T>(new canceller_task<promise_task_impl<T>, Canceller>(canceller, m_buffer));
		});
	}

	friend task<T> wait_for(promise_base const & p)
	{
		return p.wait_for();
	}

	template <typename Canceller>
	friend task<T> wait_for(promise_base const & p, Canceller const & canceller)
	{
		return p.wait_for(canceller);
	}

protected:
	shared_circular_buffer<task<T>, 1> * m_buffer;
};

} // namespace detail

template <typename T>
class promise
	: public detail::promise_base<T>
{
public:
	void set_value(T && t) const
	{
		this->m_buffer->push_back(task<T>::from_value(std::move(t)));
	}

	void set_value(T const & t) const
	{
		this->m_buffer->push_back(task<T>::from_value(t));
	}
};

template <>
class promise<void>
	: public detail::promise_base<void>
{
public:
	void set_value() const
	{
		this->m_buffer->push_back(task<void>::from_value());
	}
};

} // namespace yb

#endif // LIBYB_ASYNC_FUTURE_HPP
