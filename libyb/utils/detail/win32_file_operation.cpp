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

task<size_t> win32_file_operation::ioctl(HANDLE hFile, DWORD dwControlCode, void const * in_data, size_t in_len, void * out_data, size_t out_len)
{
	return async::fix_affinity().then([this, hFile, dwControlCode, in_data, in_len, out_data, out_len]() {
		return this->ioctl_with_affinity(hFile, dwControlCode, in_data, in_len, out_data, out_len);
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
