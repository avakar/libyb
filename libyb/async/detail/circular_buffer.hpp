#ifndef LIBYB_ASYNC_DETAIL_CIRCULAR_BUFFER_HPP
#define LIBYB_ASYNC_DETAIL_CIRCULAR_BUFFER_HPP

#include <type_traits>
#include <cstddef>

namespace yb {

template <typename T, size_t Capacity>
class circular_buffer
{
public:
	typedef T value_type;
	static size_t const static_capacity = Capacity;

	circular_buffer()
		: m_first(0), m_last(0), m_size(0)
	{
	}

	~circular_buffer()
	{
		this->clear();
	}

	bool empty() const
	{
		return m_size == 0;
	}

	bool full() const
	{
		return m_size == static_capacity;
	}

	size_t size() const
	{
		return m_size;
	}

	size_t capacity() const
	{
		return static_capacity;
	}

	void clear()
	{
		while (!this->empty())
			this->pop_front();
	}

	value_type & front()
	{
		assert(m_size > 0);
		return *this->at(m_first);
	}

	value_type const & front() const
	{
		assert(m_size > 0);
		return *this->at(m_first);
	}

	void pop_front()
	{
		assert(m_size > 0);
		this->at(m_first)->~value_type();
		m_first = next(m_first);
		--m_size;
	}

	T pop_front_move()
	{
		T res(std::move(this->front()));
		this->pop_front();
		return std::move(res);
	}

	void push_back(value_type const & v)
	{
		assert(m_size < static_capacity);
		new(this->at(m_last)) T(v);
		m_last = next(m_last);
		++m_size;
	}

	void push_back(value_type && v)
	{
		assert(m_size < static_capacity);
		new(this->at(m_last)) T(std::move(v));
		m_last = next(m_last);
		++m_size;
	}

	template <typename U>
	void emplace_back(U && v)
	{
		assert(m_size < static_capacity);
		new(this->at(m_last)) T(std::forward<U>(v));
		m_last = next(m_last);
		++m_size;
	}

private:
	T * at(size_t index = 0) { return reinterpret_cast<T *>(&m_value) + index; }
	T const * at(size_t index = 0) const { return reinterpret_cast<T *>(&m_value) + index; }

	static size_t next(size_t i)
	{
		return i + 1 == static_capacity? 0: i + 1;
	}

	size_t m_first;
	size_t m_last;
	size_t m_size;
	typename std::aligned_storage<
		sizeof(T[Capacity]),
		std::alignment_of<T[Capacity]>::value
		>::type m_value;
};

template <typename T>
class circular_buffer<T, 1>
{
public:
	typedef T value_type;
	static size_t const static_capacity = 1;

	circular_buffer()
		: m_has_value(false)
	{
	}

	~circular_buffer()
	{
		this->clear();
	}

	bool empty() const
	{
		return !m_has_value;
	}

	bool full() const
	{
		return m_has_value;
	}

	size_t size() const
	{
		return m_has_value;
	}

	size_t capacity() const
	{
		return static_capacity;
	}

	void clear()
	{
		if (m_has_value)
			this->pop_front();
	}

	value_type & front()
	{
		assert(m_has_value);
		return *this->at();
	}

	value_type const & front() const
	{
		assert(m_has_value);
		return *this->at();
	}

	void pop_front()
	{
		assert(m_has_value);
		this->at()->~value_type();
		m_has_value = false;
	}

	T pop_front_move()
	{
		T res(std::move(this->front()));
		this->pop_front();
		return std::move(res);
	}

	void push_back(value_type const & v)
	{
		assert(!m_has_value);
		new(this->at()) T(v);
		m_has_value = true;
	}

	void push_back(value_type && v)
	{
		assert(!m_has_value);
		new(this->at()) T(std::move(v));
		m_has_value = true;
	}

	template <typename U>
	void emplace_back(U && v)
	{
		assert(!m_has_value);
		new(this->at()) T(std::forward<U>(v));
		m_has_value = true;
	}

private:
	T * at() { return reinterpret_cast<T *>(&m_value); }
	T const * at() const { return reinterpret_cast<T *>(&m_value); }

	bool m_has_value;
	typename std::aligned_storage<
		sizeof(T),
		std::alignment_of<T>::value
		>::type m_value;
};

template <typename T, size_t Capacity>
class shared_circular_buffer
	: public circular_buffer<T, Capacity>
{
public:
	shared_circular_buffer()
		: m_refcount(1)
	{
	}

	void addref()
	{
		++m_refcount;
	}

	void release()
	{
		if (--m_refcount == 0)
			delete this;
	}

private:
	~shared_circular_buffer()
	{
	}

	int m_refcount;
};

} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_CIRCULAR_BUFFER_HPP
