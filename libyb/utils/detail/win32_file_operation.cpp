#include "win32_file_operation.hpp"
#include "win32_overlapped.hpp"
#include "../../async/detail/win32_handle_task.hpp"
using namespace yb;
using namespace yb::detail;

task<size_t> win32_file_operation::ioctl_with_affinity(HANDLE hFile, DWORD dwControlCode, void const * in_data, size_t in_len, void * out_data, size_t out_len)
{
	DWORD dwTransferred;
	if (!DeviceIoControl(hFile, dwControlCode, (void *)in_data, in_len, out_data, out_len, &dwTransferred, &m_overlapped.o))
	{
		DWORD dwError = GetLastError();
		if (dwError == ERROR_IO_PENDING)
		{
			return make_win32_handle_task(m_overlapped.o.hEvent, [this, hFile](cancel_level cl) -> bool {
				if (cl >= cl_abort)
					cancel_win32_overlapped(hFile, m_overlapped);
				return true;
			}).then([this, hFile]() -> task<size_t> {
				DWORD dwTransferred;
				if (!GetOverlappedResult(hFile, &m_overlapped.o, &dwTransferred, TRUE))
				{
					dwTransferred = 0;
					if (GetLastError() != ERROR_OPERATION_ABORTED)
						return async::raise<size_t>(std::runtime_error("GetOverlappedResult failure"));
				}
				return async::value((size_t)dwTransferred);
			});
		}
		else
		{
			return async::raise<size_t>(std::runtime_error("DeviceIoControl failed"));
		}
	}
	else
	{
		return async::value((size_t)dwTransferred);
	}
}

task<size_t> win32_file_operation::read_with_affinity(HANDLE hFile, void * buffer, size_t size)
{
	assert(hFile);

	DWORD dwTransferred;
	if (!ReadFile(hFile, buffer, size, &dwTransferred, &m_overlapped.o))
	{
		DWORD dwError = GetLastError();
		if (dwError == ERROR_IO_PENDING)
		{
			return make_win32_handle_task(m_overlapped.o.hEvent, [this, hFile](cancel_level cl) -> bool {
				if (cl < cl_abort)
					return true;

				if (HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll"))
				{
					typedef BOOL WINAPI CancelIoEx_t(HANDLE hFile, LPOVERLAPPED lpOverlapped);
					if (CancelIoEx_t * CancelIoEx = (CancelIoEx_t *)GetProcAddress(hKernel32, "CancelIoEx"))
						CancelIoEx(hFile, &m_overlapped.o);
					else
						CancelIo(hFile);
				}
				else
				{
					CancelIo(hFile);
				}

				return true;
			}).then([this, hFile]() -> task<size_t> {
				DWORD dwTransferred;
				if (!GetOverlappedResult(hFile, &m_overlapped.o, &dwTransferred, TRUE))
					throw std::runtime_error("the read operation failed"); // XXX
				return async::value((size_t)dwTransferred);
			});
		}
		else
			return async::raise<size_t>(std::runtime_error("the read operation failed")); // XXX
	}
	else
	{
		return async::value((size_t)dwTransferred);
	}
}

task<size_t> win32_file_operation::write_with_affinity(HANDLE hFile, void const * buffer, size_t size)
{
	assert(hFile);

	DWORD dwTransferred;
	if (!WriteFile(hFile, buffer, size, &dwTransferred, &m_overlapped.o))
	{
		DWORD dwError = GetLastError();
		if (dwError == ERROR_IO_PENDING)
		{
			return make_win32_handle_task(m_overlapped.o.hEvent, [this, hFile](cancel_level cl) -> bool {
				if (cl < cl_abort)
					return true;

				if (HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll"))
				{
					typedef BOOL WINAPI CancelIoEx_t(HANDLE hFile, LPOVERLAPPED lpOverlapped);
					if (CancelIoEx_t * CancelIoEx = (CancelIoEx_t *)GetProcAddress(hKernel32, "CancelIoEx"))
						CancelIoEx(hFile, &m_overlapped.o);
					else
						CancelIo(hFile);
				}
				else
				{
					CancelIo(hFile);
				}

				return true;
			}).then([this, hFile]() -> task<size_t> {
				DWORD dwTransferred;
				if (!GetOverlappedResult(hFile, &m_overlapped.o, &dwTransferred, TRUE))
					throw std::runtime_error("the write operation failed"); // XXX
				return async::value((size_t)dwTransferred);
			});
		}
		else
			return async::raise<size_t>(std::runtime_error("the write operation failed")); // XXX
	}
	else
	{
		return async::value((size_t)dwTransferred);
	}
}

task<size_t> win32_file_operation::ioctl(HANDLE hFile, DWORD dwControlCode, void const * in_data, size_t in_len, void * out_data, size_t out_len)
{
	return async::fix_affinity().then([this, hFile, dwControlCode, in_data, in_len, out_data, out_len]() {
		return this->ioctl_with_affinity(hFile, dwControlCode, in_data, in_len, out_data, out_len);
	});
}

task<size_t> win32_file_operation::read(HANDLE hFile, void * buffer, size_t size)
{
	return async::fix_affinity().then([this, hFile, buffer, size]() {
		return this->read_with_affinity(hFile, buffer, size);
	});
}

task<size_t> win32_file_operation::write(HANDLE hFile, void const * buffer, size_t size)
{
	return async::fix_affinity().then([this, hFile, buffer, size]() {
		return this->write_with_affinity(hFile, buffer, size);
	});
}

size_t win32_file_operation::sync_ioctl(HANDLE hFile, DWORD dwControlCode, void const * in_data, size_t in_len, void * out_data, size_t out_len)
{
	DWORD dwTransferred;
	if (!DeviceIoControl(hFile, dwControlCode, (void *)in_data, in_len, out_data, out_len, &dwTransferred, &m_overlapped.o))
	{
		DWORD dwError = GetLastError();
		if (dwError == ERROR_IO_PENDING)
		{
			DWORD dwTransferred;
			if (!GetOverlappedResult(hFile, &m_overlapped.o, &dwTransferred, TRUE))
				throw std::runtime_error("GetOverlappedResult failure");
			return dwTransferred;
		}
		else
		{
			throw std::runtime_error("DeviceIoControl failed");
		}
	}
	else
	{
		return dwTransferred;
	}
}
