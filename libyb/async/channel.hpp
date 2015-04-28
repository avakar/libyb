#ifndef LIBYB_ASYNC_CHANNEL_HPP
#define LIBYB_ASYNC_CHANNEL_HPP

#include "detail/channel_detail.hpp"
#include "task.hpp"
#include <stdexcept>

namespace yb {

template <typename T>
class channel_base
{
public:
	task<T> receive() const;

protected:
	typedef std::shared_ptr<detail::channel_buffer_base<T>> buffer_type;

	explicit channel_base(buffer_type buffer);
	buffer_type m_buffer;
};

template <typename T>
class channel
	: public channel_base<T>
{
public:
	template <size_t Capacity = 1>
	static channel create_finite();
	static channel create_infinite();
	static channel create_null();

	task<void> send(T const & value) const;
	task<void> send(T && value) const;

	void send_sync(T const & value) const;
	void send_sync(T && value) const;

private:
	explicit channel(buffer_type buffer);
};

template <>
class channel<void>
	: public channel_base<void>
{
public:
	template <size_t Capacity = 1>
	static channel create_finite();
	static channel create_infinite();
	static channel create_null();

	task<void> send() const;
	void send_sync() const;

	task<void> fire() const;

private:
	explicit channel(buffer_type buffer);
};

task<void> wait_for(channel<void> const & sig);

} // namespace yb

#include "detail/circular_buffer.hpp"

namespace yb {

template <typename T>
channel_base<T>::channel_base(buffer_type buffer)
	: m_buffer(buffer)
{
}

template <typename T>
task<T> channel_base<T>::receive() const
{
	return m_buffer->receive();
}

template <typename T>
template <size_t Capacity>
channel<T> channel<T>::create_finite()
{
	return channel<T>(std::make_shared<detail::channel_buffer<T, circular_buffer<task<T>, Capacity> > >());
}

template <typename T>
channel<T> channel<T>::create_infinite()
{
	return channel<T>(std::make_shared<detail::channel_buffer<T, detail::unbounded_channel_buffer<task<T>> > >());
}


template <typename T>
channel<T> channel<T>::create_null()
{
	return channel<T>(std::make_shared<detail::channel_buffer<T, detail::null_channel_buffer<task<T>> > >());
}

template <typename T>
channel<T>::channel(buffer_type buffer)
	: channel_base<T>(buffer)
{
}

template <typename T>
task<void> channel<T>::send(T const & value) const
{
	return m_buffer->send(task<T>::from_value(value));
}

template <typename T>
task<void> channel<T>::send(T && value) const
{
	return m_buffer->send(task<T>::from_value(std::move(value)));
}

template <typename T>
void channel<T>::send_sync(T const & value) const
{
	task<void> r = this->send(value);
	assert(r.has_value() || r.has_exception());
	r.rethrow();
}

template <typename T>
void channel<T>::send_sync(T && value) const
{
	task<void> r = this->send(std::move(value));
	assert(r.has_value() || r.has_exception());
	r.rethrow();
}

template <size_t Capacity>
channel<void> channel<void>::create_finite()
{
	return channel<void>(std::make_shared<detail::channel_buffer<void, circular_buffer<task<void>, Capacity> > >());
}

} // namespace yb

#endif // LIBYB_ASYNC_CHANNEL_HPP
