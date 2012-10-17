#include "task_win32.hpp"
#include "detail/win32_handle_task.hpp"

namespace yb {

task<void> make_win32_handle_task(HANDLE handle)
{
	try
	{
		return task<void>(new win32_handle_task(handle));
	}
	catch (...)
	{
		return task<void>(std::current_exception());
	}
}

} // namespace yb
