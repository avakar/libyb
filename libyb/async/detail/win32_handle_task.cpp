#include "win32_handle_task.hpp"
#include <stdexcept>

using namespace yb;

task<void> yb::async::fix_affinity()
{
	try
	{
		return task<void>::from_task(new win32_affinity_task());
	}
	catch (...)
	{
		return task<void>::from_exception(std::current_exception());
	}
}
