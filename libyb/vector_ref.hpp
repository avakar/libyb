#ifndef LIBYB_VECTOR_REF_HPP
#define LIBYB_VECTOR_REF_HPP

#include <string.h>
#include <string>
#include <vector>

namespace yb {

template <typename T>
class vector_ref
{
public:
	vector_ref()
		: first(0), last(0)
	{
	}

	vector_ref(T const * first, T const * last)
		: first(first), last(last)
	{
	}

	vector_ref(T const * first, size_t size)
		: first(first), last(first + size)
	{
	}

	vector_ref(std::vector<T> const & v)
		: first(v.data()), last(v.data() + v.size())
	{
	}

	T const * data() const { return first; }
	size_t size() const { return static_cast<size_t>(last - first); }
	bool empty() const { return first == last; }

	T const & operator[](size_t i) const { return first[i]; }

private:
	T const * first;
	T const * last;
};

typedef vector_ref<uint8_t> buffer_ref;

class string_ref
	: public vector_ref<char>
{
public:
	string_ref()
	{
	}

	string_ref(char const * first, char const * last)
		: vector_ref<char>(first, last)
	{
	}

	string_ref(char const * first, size_t size)
		: vector_ref<char>(first, size)
	{
	}

	string_ref(char const * str)
		: vector_ref<char>(str, strlen(str))
	{
	}

	string_ref(std::string const & str)
		: vector_ref<char>(str.data(), str.size())
	{
	}

	operator std::string() const
	{
		return std::string(this->data(), this->size());
	}
};

}

#endif // LIBYB_VECTOR_REF_HPP
