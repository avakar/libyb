#ifndef LIBYB_ASYNC_SIGNAL_HPP
#define LIBYB_ASYNC_SIGNAL_HPP

#include "task.hpp"

namespace yb {

class signal
{
public:
	signal();
	signal(signal const & o);
	signal(signal && o);
	~signal();

	void fire();

	friend task<void> wait_for(signal & sig);

private:
	struct signal_task;
	struct impl;
	impl * m_pimpl;
};

} // namespace yb

#endif // LIBYB_ASYNC_SIGNAL_HPP
