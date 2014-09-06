#include "channel.hpp"
using namespace yb;

channel<void> channel<void>::create_infinite()
{
	return channel<void>(std::make_shared<detail::channel_buffer<void, detail::unbounded_channel_buffer<task<void>> > >());
}

channel<void>::channel(buffer_type buffer)
: channel_base<void>(buffer)
{
}

task<void> channel<void>::send() const
{
	return this->send(task<void>::from_value());
}

void channel<void>::send_sync() const
{
	task<void> r = this->send();
	assert(r.has_value() || r.has_exception());
	r.rethrow();
}

task<void> channel<void>::fire() const
{
	return this->send();
}

task<void> yb::wait_for(channel<void> const & sig)
{
	return sig.receive();
}
