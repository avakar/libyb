#include "../sigint.hpp"
#include "../channel.hpp"
#include <windows.h>
#include <list>

static yb::channel<void> g_waiters = yb::channel<void>::create_infinite();

static BOOL WINAPI ConsoleSignalHandler(DWORD dwCtrlType)
{
	if (dwCtrlType == CTRL_C_EVENT)
	{
		g_waiters.send_sync();
		return TRUE;
	}

	return FALSE;
}

yb::task<void> yb::wait_for_sigint()
{
	::SetConsoleCtrlHandler(&ConsoleSignalHandler, TRUE);
	return g_waiters.receive().continue_with([](yb::task<void>) {
		::SetConsoleCtrlHandler(&ConsoleSignalHandler, FALSE);
		return yb::async::value();
	});
}
