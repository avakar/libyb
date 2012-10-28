#ifndef LIBYB_VECTOR_REF_HPP
#define LIBYB_VECTOR_REF_HPP

#include <string.h>
#include <string>
#include <vector>

namespace yb {

template <typename T>
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

private:
	T const * first;
	T const * last;
};

template <typename T>
class vector_ref
	: public vector_ref_base<T>
{
public:
	vector_ref()
		: vector_ref_base<T>()
	{
	}

	vector_ref(T const * first, T const * last)
		: vector_ref_base<T>(first, last)
	{
	}

	vector_ref(T const * first, size_t size)
		: vector_ref_base<T>(first, size)
	{
	}

	vector_ref(std::vector<T> const & v)
		: vector_ref_base<T>(v)
	{
	}
};

template <>
class vector_ref<uint8_t>
	: public vector_ref_base<uint8_t>
{
public:
	vector_ref()
		: vector_ref_base<uint8_t>()
	{
	}

	vector_ref(uint8_t const * first, uint8_t const * last)
		: vector_ref_base<uint8_t>(first, last)
	{
	}

	vector_ref(uint8_t const * first, size_t size)
		: vector_ref_base<uint8_t>(first, size)
	{
	}

	vector_ref(std::vector<uint8_t> const & v)
		: vector_ref_base<uint8_t>(v)
	{
	}

	template <size_t N>
	vector_ref(uint8_t const (&v)[N])
		: vector_ref_base<uint8_t>(v, N)
	{
	}
};

typedef vector_ref<uint8_t> buffer_ref;

template <>
class vector_ref<char>
	: public vector_ref_base<char>
{
public:
	vector_ref()
		: vector_ref_base<char>()
	{
	}

	vector_ref(char const * first, char const * last)
		: vector_ref_base<char>(first, last)
	{
	}

	vector_ref(char const * first, size_t size)
		: vector_ref_base<char>(first, size)
	{
	}

	vector_ref(std::vector<char> const & v)
		: vector_ref_base<char>(v)
	{
	}

	vector_ref(char const * str)
		: vector_ref_base<char>(str, strlen(str))
	{
	}

	vector_ref(std::string const & str)
		: vector_ref_base<char>(str.data(), str.size())
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
