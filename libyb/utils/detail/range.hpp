#ifndef LIBYB_UTILS_DETAIL_RANGE_HPP
#define LIBYB_UTILS_DETAIL_RANGE_HPP

namespace yb {
namespace detail {

template <typename R>
class range_iterator
{
public:
	typedef typename R::value_type value_type;
	typedef typename R::reference reference;

	explicit range_iterator(R const * r)
		: m_r(r)
	{
	}

	reference operator*() const
	{
		assert(m_r && !m_r->empty());
		return m_r->front();
	}

	value_type * operator->() const
	{
		assert(m_r && !m_r->empty());
		return &m_r->front();
	}

	range_iterator & operator++()
	{
		assert(m_r && !m_r->empty());
		m_r->pop_front();
		return *this;
	}

	range_iterator & operator++(int)
	{
		assert(m_r && !m_r->empty());
		m_r->pop_front();
		return *this;
	}

	friend bool operator==(range_iterator const & lhs, range_iterator const & rhs)
	{
		return lhs.m_r == rhs.m_r
			|| (lhs.m_r && !rhs.m_r && lhs.m_r->empty())
			|| (!lhs.m_r && rhs.m_r && rhs.m_r->empty());
	}

	friend bool operator!=(range_iterator const & lhs, range_iterator const & rhs)
	{
		return !(lhs == rhs);
	}

private:
	R const * m_r;
};

} // namespace detail
} // namespace yb

#endif // LIBYB_UTILS_DETAIL_RANGE_HPP
