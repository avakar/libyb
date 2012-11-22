#include "timer.hpp"
using namespace yb;

task<void> yb::wait_ms(int milliseconds)
{
	std::shared_ptr<timer> tmr(new timer());
	return tmr->wait_ms(milliseconds).follow_with([tmr]{});
}
