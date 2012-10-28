#include "timer.hpp"
#include "detail/win32_handle_task.hpp"
#include <stdexcept>
using namespace yb;

struct timer::impl
{
	HANDLE hTimer;
};

timer::timer()
	: m_pimpl(new impl())
{
	m_pimpl->hTimer = CreateWaitableTimer(0, TRUE, 0);
	if (!m_pimpl->hTimer)
		throw std::runtime_error("couldn't create the timer");
}

timer::~timer()
{
	CloseHandle(m_pimpl->hTimer);
}

task<void> timer::wait_ms(int milliseconds)
{
	LARGE_INTEGER tout;
	tout.QuadPart = milliseconds * -10*1000ull;
	if (!SetWaitableTimer(m_pimpl->hTimer, &tout, 0, 0, 0, FALSE))
		return async::raise<void>(std::runtime_error("couldn't set the timer"));

	return make_win32_handle_task(m_pimpl->hTimer, [this](cancel_level_t) -> bool {
		CancelWaitableTimer(m_pimpl->hTimer);
		return false;
	});
}
