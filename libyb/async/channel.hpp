#ifndef LIBYB_ASYNC_CHANNEL_HPP
#define LIBYB_ASYNC_CHANNEL_HPP

#include "detail/channel_detail.hpp"
#include "task.hpp"
#include <stdexcept>

namespace yb {

template <typename T, size_t Capacity>
class channel_base
{
public:
	static size_t const static_capacity = Capacity;

	~channel_base();
	channel_base(channel_base const & o);
	channel_base & operator=(channel_base const & o);

	task<T> receive();
	task<void> send(task_result<T> && r);
	task<void> send(task_result<T> const & r);

protected:
	typedef shared_circular_buffer<task_result<T>, static_capacity> buffer_type;

	explicit channel_base(buffer_type * buffer);

	buffer_type * m_buffer;
};

template <typename T, size_t Capacity = 1>
class channel
	: public channel_base<T, Capacity>
{
public:
	static channel create();

	task<void> send(T const & value);
	task<void> send(T && value);
	using channel_base<T, Capacity>::send;

private:
	explicit channel(buffer_type * buffer);
};

template <size_t Capacity>
class channel<void, Capacity>
	: public channel_base<void, Capacity>
{
public:
	static channel create();

	task<void> send();
	using channel_base<void, Capacity>::send;

	task<void> fire();

private:
	explicit channel(buffer_type * buffer);
};

} // namespace yb

namespace yb {

template <typename T, size_t Capacity>
channel_base<T, Capacity>::channel_base(buffer_type * buffer)
	: m_buffer(buffer)
{
}

template <typename T, size_t Capacity>
channel_base<T, Capacity>::~channel_base()
{
	m_buffer->release();
}

template <typename T, size_t Capacity>
channel_base<T, Capacity>::channel_base(channel_base const & o)
	: m_buffer(o.m_buffer)
{
	m_buffer->addref();
}

template <typename T, size_t Capacity>
channel_base<T, Capacity> & channel_base<T, Capacity>::operator=(channel_base const & o)
{
	o.m_buffer->addref();
	m_buffer->release();
	m_buffer = o.m_buffer;
	return *this;
}

template <typename T, size_t Capacity>
task<T> channel_base<T, Capacity>::receive()
{
	try
	{
		if (m_buffer->empty())
			return task<T>(new channel_receive_task<T, Capacity>(m_buffer));
		else
			return async::result(m_buffer->pop_front_move());
	}
	catch (...)
	{
		return async::raise<T>();
	}
}

template <typename T, size_t Capacity>
task<void> channel_base<T, Capacity>::send(task_result<T> && r)
{
	try
	{
		if (!m_buffer->full())
		{
			m_buffer->push_back(std::move(r));
			return async::value();
		}
		else
		{
			return task<void>(new channel_send_task<T, Capacity>(m_buffer, std::move(r)));
		}
	}
	catch (...)
	{
		return async::raise<void>();
	}
}

template <typename T, size_t Capacity>
task<void> channel_base<T, Capacity>::send(task_result<T> const & r)
{
	return this->send(task_result<T>(r));
}

template <typename T, size_t Capacity>
channel<T, Capacity> channel<T, Capacity>::create()
{
	return channel<T, Capacity>(new buffer_type());
}

template <typename T, size_t Capacity>
channel<T, Capacity>::channel(buffer_type * buffer)
	: channel_base<T, Capacity>(buffer)
{
}

template <typename T, size_t Capacity>
task<void> channel<T, Capacity>::send(T const & value)
{
	return this->send(task_result<T>(value));
}

template <typename T, size_t Capacity>
task<void> channel<T, Capacity>::send(T && value)
{
	return this->send(task_result<T>(std::move(value)));
}

template <size_t Capacity>
channel<void, Capacity> channel<void, Capacity>::create()
{
	return channel<void>(new buffer_type());
}

template <size_t Capacity>
channel<void, Capacity>::channel(buffer_type * buffer)
	: channel_base<void, Capacity>(buffer)
{
}

template <size_t Capacity>
task<void> channel<void, Capacity>::send()
{
	return this->send(task_result<void>());
}

template <size_t Capacity>
task<void> channel<void, Capacity>::fire()
{
	return this->send();
}

template <size_t Capacity>
task<void> wait_for(channel<void, Capacity> & sig)
{
	return sig.receive();

}

} // namespace yb

#endif // LIBYB_ASYNC_CHANNEL_HPP
