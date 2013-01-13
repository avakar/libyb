#include "win32_handle_task.hpp"
using namespace yb;

task<void> yb::async::fix_affinity()
{
	try
	{
		return task<void>(new win32_affinity_task());
	}
	catch (...)
	{
		return std::current_exception();
	}
}
