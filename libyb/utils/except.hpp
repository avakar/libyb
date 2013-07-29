#ifndef LIBYB_UTILS_EXCEPT_HPP
#define LIBYB_UTILS_EXCEPT_HPP

#include <exception>
#include <utility>

namespace yb {

template <typename E>
std::exception_ptr make_exception_ptr(E && e)
{
#if _MSC_VER == 1600
	return std::copy_exception(std::forward<E>(e));
#else
	return std::make_exception_ptr(std::forward<E>(e));
#endif
}

}

#endif
