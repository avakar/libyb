#ifndef LIBYB_UTILS_DETAIL_UNIX_SYSTEM_ERROR_HPP
#define LIBYB_UTILS_DETAIL_UNIX_SYSTEM_ERROR_HPP

#include <stdexcept>

namespace yb {
namespace detail {

class unix_system_error
	: public std::runtime_error
{
public:
	explicit unix_system_error(int error_number)
		: std::runtime_error(std::string()), m_errno(error_number)
	{
	}

	int error_number() const
	{
		return m_errno;
	}

private:
	int m_errno;
};

} // namespace detail
} // namespace yb

#endif // LIBYB_UTILS_DETAIL_UNIX_SYSTEM_ERROR_HPP
