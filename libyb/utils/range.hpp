#ifndef LIBYB_RANGE_HPP
#define LIBYB_RANGE_HPP

#include <iterator>
#include <cassert>
#include "detail/range.hpp"

namespace yb {

template <typename I>
class range_adaptor
{
public:
	typedef typename std::iterator_traits<I>::value_type value_type;
	typedef typename std::iterator_traits<I>::reference reference;
	typedef I iterator;

	range_adaptor()
	{
	}

	range_adaptor(I first, I last)
		: m_first(first), m_last(last)
	{
	}

	iterator begin() const
	{
		return m_first;
	}

	iterator end() const
	{
		return m_last;
	}

	bool empty() const
	{
		return m_first == m_last;
	}

	void pop_front()
	{
		assert(!this->empty());
		++m_first;
	}

	reference front() const
	{
		assert(!this->empty());
		return *m_first;
	}

private:
	I m_first;
	I m_last;
};

template <typename Impl>
class range
{
public:
	typedef typename Impl::value_type value_type;
	typedef typename Impl::reference reference;
	typedef detail::range_iterator<range> iterator;

	range()
	{
	}

	explicit range(Impl impl)
		: m_impl(std::move(impl))
	{
	}

	iterator begin() const
	{
		return iterator(this);
	}

	iterator end() const
	{
		return iterator(nullptr);
	}

	bool empty() const
	{
		return m_impl.empty();
	}

	void pop_front()
	{
		assert(!this->empty());
		return m_impl.pop_front();
	}

	reference front() const
	{
		assert(!this->empty());
		return m_impl.front();
	}

private:
	Impl m_impl;
};


} // namespace yb

#endif // LIBYB_RANGE_HPP
