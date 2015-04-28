#ifndef LIBYB_ASYNC_PROMISE_HPP
#define LIBYB_ASYNC_PROMISE_HPP

#include "task.hpp"
#include "detail/notification_event.hpp"

namespace yb {

template <typename T>
class promise
{
public:
	promise()
	{
	}

	task<T> wait()
	{
		return m_event.wait().then([this]() {
			return std::move(m_value);
		});
	}

	template <typename U>
	void set(U && u)
	{
		m_value = yb::task<T>::from_value(std::move(u));
		m_event.set();
	}

private:
	yb::notification_event m_event;
	task<T> m_value;

	promise(promise const &);
	promise & operator=(promise const &);
};

template <>
class promise<void>
{
public:
	promise()
	{
	}

	task<void> wait()
	{
		return m_event.wait();
	}

	void set()
	{
		m_event.set();
	}

private:
	yb::notification_event m_event;

	promise(promise const &);
	promise & operator=(promise const &);
};

} // namespace yb

#endif // LIBYB_ASYNC_PROMISE_HPP
