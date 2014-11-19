#include "../runner.hpp"
using namespace yb;

void runner::run_forever() throw()
{
	this->run_until(0);
}
