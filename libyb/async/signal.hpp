#ifndef LIBYB_ASYNC_SIGNAL_HPP
#define LIBYB_ASYNC_SIGNAL_HPP

#include "channel.hpp"

namespace yb {

class signal
	: public channel<void>
{
public:
	void fire()
	{
		this->send();
	}

	friend task<void> wait_for(signal & sig)
	{
		return sig.receive();
	}
};

} // namespace yb

#endif // LIBYB_ASYNC_SIGNAL_HPP
