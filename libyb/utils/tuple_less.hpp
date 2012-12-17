#ifndef LIBYB_UTILS_TUPLE_LESS_HPP
#define LIBYB_UTILS_TUPLE_LESS_HPP

namespace yb {
namespace detail {

class tuple_comparator
{
public:
	tuple_comparator()
		: m_state(st_equal)
	{
	}

	template <typename T>
	tuple_comparator & operator()(T const & lhs, T const & rhs)
	{
		if (m_state == st_equal)
		{
			if (lhs < rhs)
				m_state = st_less;
			else if (rhs < lhs)
				m_state = st_greater;
		}
		return *this;
	}

	operator bool() const
	{
		return m_state == st_less;
	}

private:
	enum { st_equal, st_less, st_greater } m_state;
};

} // namespace detail

template <typename T>
detail::tuple_comparator tuple_less(T const & lhs, T const & rhs)
{
	return detail::tuple_comparator()(lhs, rhs);
}

} // namespace yb

#endif // LIBYB_UTILS_TUPLE_LESS_HPP
