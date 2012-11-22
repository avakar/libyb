#ifndef LIBYB_ASYNC_CANCEL_EXCEPTION_HPP
#define LIBYB_ASYNC_CANCEL_EXCEPTION_HPP

#include "cancel_level.hpp"
#include <exception>

namespace yb {

class task_cancelled
	: public std::exception
{
public:
	explicit task_cancelled(cancel_level cl = cl_none)
		: m_cl(cl)
	{
	}

	const char * what() const throw()
	{
		return "cancelled";
	}

private:
	cancel_level m_cl;
};

} // namespace yb

#endif // LIBYB_ASYNC_CANCEL_EXCEPTION_HPP
