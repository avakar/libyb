#ifndef LIBYB_ASYNC_DETAIL_TASK_RESULT_HPP
#define LIBYB_ASYNC_DETAIL_TASK_RESULT_HPP

#include <stddef.h>

namespace yb {
namespace detail {

template <size_t A, size_t B, size_t C>
struct yb_max
{
	static size_t const _ab = A > B? A: B;
	static size_t const value = _ab > C? _ab: C;
};

template <size_t A, size_t B>
struct yb_gcd
{
	static size_t const value = yb_gcd<B, A % B>::value;
};

template <size_t A>
struct yb_gcd<A, 0>
{
	static size_t const value = A;
};

template <size_t A, size_t B, size_t C>
struct yb_lcm
{
	static size_t const _ab = A * B / yb_gcd<A, B>::value;
	static size_t const value = _ab * C / yb_gcd<_ab, C>::value;
};

} // namespace detail
} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_TASK_RESULT_HPP
