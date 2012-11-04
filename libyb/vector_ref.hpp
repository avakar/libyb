#ifndef LIBYB_VECTOR_REF_HPP
#define LIBYB_VECTOR_REF_HPP

#include <string.h>
#include <string>
#include <vector>
#include <algorithm>
#include <stdint.h>

namespace yb {

template <typename T, typename Derived>
class vector_ref_base
{
public:
	vector_ref_base()
		: first(0), last(0)
	{
	}

	vector_ref_base(T const * first, T const * last)
		: first(first), last(last)
	{
	}

	vector_ref_base(T const * first, size_t size)
		: first(first), last(first + size)
	{
	}

	vector_ref_base(std::vector<T> const & v)
		: first(v.data()), last(v.data() + v.size())
	{
	}

	T const * begin() const { return first; }
	T const * end() const { return last; }

	T const * data() const { return first; }
	size_t size() const { return static_cast<size_t>(last - first); }
	bool empty() const { return first == last; }

	T const & operator[](size_t i) const { return first[i]; }

	friend Derived operator+(vector_ref_base const & lhs, size_t rhs)
	{
		return Derived(lhs.begin() + (std::min)(lhs.size(), rhs), lhs.end());
	}

	Derived & operator+=(size_t offset)
	{
		first += (std::min)(this->size(), offset);
		return static_cast<Derived &>(*this);
	}

	friend bool operator==(vector_ref_base const & lhs, vector_ref_base const & rhs)
	{
		return (lhs.last - lhs.first) == (rhs.last - rhs.first)
			&& std::equal(lhs.first, lhs.last, rhs.first);
	}

	friend bool operator!=(vector_ref_base const & lhs, vector_ref_base const & rhs)
	{
		return !(lhs == rhs);
	}

	friend bool operator<(vector_ref_base const & lhs, vector_ref_base const & rhs)
	{
		return std::lexicographical_compare(lhs.first, lhs.last, rhs.first, rhs.last);
	}

	friend bool operator>(vector_ref_base const & lhs, vector_ref_base const & rhs)
	{
		return rhs < lhs;
	}

	friend bool operator<=(vector_ref_base const & lhs, vector_ref_base const & rhs)
	{
		return !(rhs < lhs);
	}

	friend bool operator>=(vector_ref_base const & lhs, vector_ref_base const & rhs)
	{
		return !(lhs < rhs);
	}

private:
	T const * first;
	T const * last;
};

template <typename T>
class vector_ref
	: public vector_ref_base<T, vector_ref<T>>
{
	typedef vector_ref_base<T, vector_ref<T>> base_type;
public:
	vector_ref()
		: base_type()
	{
	}

	vector_ref(T const * first, T const * last)
		: base_type(first, last)
	{
	}

	vector_ref(T const * first, size_t size)
		: base_type(first, size)
	{
	}

	vector_ref(std::vector<T> const & v)
		: base_type(v)
	{
	}
};

template <>
class vector_ref<uint8_t>
	: public vector_ref_base<uint8_t, vector_ref<uint8_t>>
{
	typedef vector_ref_base<uint8_t, vector_ref<uint8_t>> base_type;
public:
	vector_ref()
		: base_type()
	{
	}

	vector_ref(uint8_t const * first, uint8_t const * last)
		: base_type(first, last)
	{
	}

	vector_ref(uint8_t const * first, size_t size)
		: base_type(first, size)
	{
	}

	vector_ref(std::vector<uint8_t> const & v)
		: base_type(v)
	{
	}

	template <size_t N>
	vector_ref(uint8_t const (&v)[N])
		: base_type(v, N)
	{
	}
};

typedef vector_ref<uint8_t> buffer_ref;

template <>
class vector_ref<char>
	: public vector_ref_base<char, vector_ref<char>>
{
	typedef vector_ref_base<char, vector_ref<char>> base_type;
public:
	vector_ref()
		: base_type()
	{
	}

	vector_ref(char const * first, char const * last)
		: base_type(first, last)
	{
	}

	vector_ref(char const * first, size_t size)
		: base_type(first, size)
	{
	}

	vector_ref(std::vector<char> const & v)
		: base_type(v)
	{
	}

	vector_ref(char const * str)
		: base_type(str, strlen(str))
	{
	}

	vector_ref(std::string const & str)
		: base_type(str.data(), str.size())
	{
	}

	operator std::string() const
	{
		return std::string(this->data(), this->size());
	}
};

typedef vector_ref<char> string_ref;

}

#endif // LIBYB_VECTOR_REF_HPP
