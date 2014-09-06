#ifndef LIBYB_UTILS_DETAIL_WIN32_SYSTEM_ERROR_HPP
#define LIBYB_UTILS_DETAIL_WIN32_SYSTEM_ERROR_HPP

#include <stdint.h>
#include <stdexcept>

namespace yb {
namespace detail {

class win32_system_error
	: public std::runtime_error
{
public:
	explicit win32_system_error(uint32_t error_number = 0)
		: std::runtime_error(std::string()), m_error_number(error_number)
	{
	}

	int error_number() const
	{
		return m_error_number;
	}

private:
	int m_error_number;
};

} // namespace detail
} // namespace yb

#endif // LIBYB_UTILS_DETAIL_WIN32_SYSTEM_ERROR_HPP
