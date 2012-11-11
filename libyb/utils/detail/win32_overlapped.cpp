#include "win32_overlapped.hpp"

void yb::detail::cancel_win32_overlapped(HANDLE hFile, OVERLAPPED * o)
{
	if (HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll"))
	{
		typedef BOOL WINAPI CancelIoEx_t(HANDLE hFile, LPOVERLAPPED lpOverlapped);
		if (CancelIoEx_t * CancelIoEx = (CancelIoEx_t *)GetProcAddress(hKernel32, "CancelIoEx"))
			CancelIoEx(hFile, o);
		else
			CancelIo(hFile);
	}
	else
	{
		CancelIo(hFile);
	}
}

void yb::detail::cancel_win32_overlapped(HANDLE hFile, win32_overlapped & o)
{
	cancel_win32_overlapped(hFile, &o.o);
}
