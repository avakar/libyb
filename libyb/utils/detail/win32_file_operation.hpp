#ifndef LIBYB_UTILS_DETAIL_WIN32_FILE_OPERATION_HPP
#define LIBYB_UTILS_DETAIL_WIN32_FILE_OPERATION_HPP

#include "../../async/task.hpp"
#include "win32_overlapped.hpp"
#include <windows.h>

namespace yb {
namespace detail {

class win32_file_operation
{
public:
	task<size_t> ioctl(HANDLE hFile, DWORD dwControlCode, void const * in_data, size_t in_len, void * out_data, size_t out_len);
	size_t sync_ioctl(HANDLE hFile, DWORD dwControlCode, void const * in_data, size_t in_len, void * out_data, size_t out_len);

private:
	task<size_t> ioctl_with_affinity(HANDLE hFile, DWORD dwControlCode, void const * in_data, size_t in_len, void * out_data, size_t out_len);

	detail::win32_overlapped m_overlapped;
};

} // namespace detail
} // namespace yb

#endif // LIBYB_UTILS_DETAIL_WIN32_FILE_OPERATION_HPP
