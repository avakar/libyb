#ifndef LIBYB_ASYNC_TIMER_HPP
#define LIBYB_ASYNC_TIMER_HPP

#include "task.hpp"
#include <memory>

namespace yb {

class timer
{
public:
	timer();
	~timer();

	task<void> wait_ms(int milliseconds);

private:
	struct impl;
	std::unique_ptr<impl> m_pimpl;

	timer(timer const &);
	timer & operator=(timer const &);
};

task<void> wait_ms(int milliseconds);

} // namespace yb

#endif // LIBYB_ASYNC_TIMER_HPP
