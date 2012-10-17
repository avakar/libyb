#ifndef LIBYB_ASYNC_DETAIL_TASK_RESULT_HPP
#define LIBYB_ASYNC_DETAIL_TASK_RESULT_HPP

#include <stddef.h>

namespace yb {
namespace detail {

template <size_t A, size_t B, bool A_less_than_B>
struct yb_max_impl
{
	static size_t const value = B;
};

template <size_t A, size_t B>
struct yb_max_impl<A, B, false>
{
	static size_t const value = A;
};

template <size_t A, size_t B>
struct yb_max
{
	static size_t const value = yb_max_impl<A, B, (A < B)>::value;
};

template <size_t A, size_t B>
struct yb_gcd
{
	static size_t const value = yb_gcd<B, A - B*(A/B)>::value;
};

template <size_t A>
struct yb_gcd<A, 0>
{
	static size_t const value = A;
};

template <size_t A, size_t B>
struct yb_lcm
{
	static size_t const value = A * B * yb_gcd<A, B>::value;
};

} // namespace detail
} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_TASK_RESULT_HPP
