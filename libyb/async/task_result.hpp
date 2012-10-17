#ifndef LIBYB_ASYNC_TASK_RESULT_HPP
#define LIBYB_ASYNC_TASK_RESULT_HPP

#include <exception> // exception_ptr
#include <type_traits> // aligned_storage, alignment_of

#include "detail/task_result.hpp"

namespace yb {

template <typename T>
class task_result
{
public:
	typedef T value_type;

	task_result(value_type const & v) throw();
	task_result(value_type && v) throw();
	task_result(std::exception_ptr e) throw();

	task_result(task_result const & o) throw();
	task_result(task_result && o) throw();
	~task_result() throw();

	task_result & operator=(task_result && o);

	bool has_value() const throw();
	bool has_exception() const throw();

	value_type get();
	std::exception_ptr exception() const throw();

	void rethrow();

private:
	T & as_value() { return reinterpret_cast<T &>(m_storage); }
	T const & as_value() const { return reinterpret_cast<T const &>(m_storage); }
	std::exception_ptr & as_exception() { return reinterpret_cast<std::exception_ptr &>(m_storage); }
	std::exception_ptr const & as_exception() const { return reinterpret_cast<std::exception_ptr const &>(m_storage); }

	bool m_has_value;
	typename std::aligned_storage<
		detail::yb_max<sizeof(T), sizeof(std::exception_ptr)>::value,
		detail::yb_lcm<
			std::alignment_of<T>::value,
			std::alignment_of<std::exception_ptr>::value
			>::value
		>::type m_storage;

	task_result & operator=(task_result const & o);
};

template <>
class task_result<void>
{
public:
	typedef void value_type;

	task_result() throw();
	task_result(std::exception_ptr e) throw();

	task_result(task_result const & o) throw();
	task_result(task_result && o) throw();

	task_result & operator=(task_result && o);

	bool has_value() const throw();
	bool has_exception() const throw();

	void get();
	std::exception_ptr exception() const throw();

	void rethrow();

private:
	std::exception_ptr m_exception;

	task_result & operator=(task_result const & o);
};

} // namespace yb

#include <utility> //move
#include <new> //placement new
#include <cassert>

namespace yb {

template <typename T>
task_result<T>::task_result(value_type const & v)
	: m_has_value(true)
{
	new(&m_storage) value_type(v);
}

template <typename T>
task_result<T>::task_result(value_type && v) throw()
	: m_has_value(true)
{
	try
	{
		new(&m_storage) value_type(std::move(v));
	}
	catch (...)
	{
		m_has_value = false;
		new(&m_storage) std::exception_ptr(std::current_exception());
	}
}

template <typename T>
task_result<T>::task_result(std::exception_ptr e) throw()
	: m_has_value(false)
{
	assert(!(e == nullptr));
	new(&m_storage) std::exception_ptr(std::move(e));
}

template <typename T>
task_result<T>::task_result(task_result const & o) throw()
	: m_has_value(o.m_has_value)
{
	if (m_has_value)
	{
		try
		{
			new(&m_storage) value_type(o.as_value());
		}
		catch (...)
		{
			m_has_value = false;
			new(&m_storage) std::exception_ptr(std::current_exception());
		}
	}
	else
	{
		new(&m_storage) std::exception_ptr(o.as_exception());
	}
}

template <typename T>
task_result<T>::task_result(task_result && o) throw()
	: m_has_value(o.m_has_value)
{
	if (m_has_value)
	{
		try
		{
			new(&m_storage) value_type(std::move(o.as_value()));
		}
		catch (...)
		{
			m_has_value = false;
			new(&m_storage) std::exception_ptr(std::current_exception());
		}
	}
	else
	{
		new(&m_storage) std::exception_ptr(std::move(o.as_exception()));
	}
}

template <typename T>
task_result<T>::~task_result() throw()
{
	using std::exception_ptr;

	if (m_has_value)
		this->as_value().~value_type();
	else
		this->as_exception().~exception_ptr();
}

template <typename T>
task_result<T> & task_result<T>::operator=(task_result<T> && o)
{
	using std::exception_ptr;

	if (m_has_value && o.m_has_value)
	{
		try
		{
			this->as_value() = std::move(o.as_value());
		}
		catch (...)
		{
			this->as_value().~value_type();
			m_has_value = false;
			new(&m_storage) std::exception_ptr(std::current_exception());
		}
	}
	else if (!m_has_value && !o.m_has_value)
	{
		this->as_exception() = std::move(o.as_exception());
	}
	else if (m_has_value)
	{
		this->as_value().~value_type();
		m_has_value = false;
		new(&m_storage) std::exception_ptr(std::move(o.as_exception()));
	}
	else
	{
		this->as_exception().~exception_ptr();
		try
		{
			new(&m_storage) value_type(std::move(o.as_value()));
			m_has_value = true;
		}
		catch (...)
		{
			new(&m_storage) std::exception_ptr(std::current_exception());
		}
	}

	return *this;
}

template <typename T>
bool task_result<T>::has_value() const throw()
{
	return m_has_value;
}

template <typename T>
bool task_result<T>::has_exception() const throw()
{
	return !m_has_value;
}

template <typename T>
T task_result<T>::get()
{
	this->rethrow();
	return std::move(this->as_value());
}

template <typename T>
std::exception_ptr task_result<T>::exception() const throw()
{
	return this->as_exception();
}

template <typename T>
void task_result<T>::rethrow()
{
	if (!m_has_value)
		std::rethrow_exception(this->as_exception());
}

inline task_result<void>::task_result() throw()
	: m_exception()
{
}

inline task_result<void>::task_result(std::exception_ptr e) throw()
	: m_exception(std::move(e))
{
}

inline task_result<void>::task_result(task_result const & o) throw()
	: m_exception(o.m_exception)
{
}

inline task_result<void>::task_result(task_result && o) throw()
	: m_exception(std::move(o.m_exception))
{
}

inline task_result<void> & task_result<void>::operator=(task_result<void> && o)
{
	m_exception = std::move(o.m_exception);
	return *this;
}

inline bool task_result<void>::has_value() const throw()
{
	return m_exception == nullptr;
}

inline bool task_result<void>::has_exception() const throw()
{
	return !(m_exception == nullptr);
}

inline std::exception_ptr task_result<void>::exception() const throw()
{
	return this->m_exception;
}

inline void task_result<void>::get()
{
	this->rethrow();
}

inline void task_result<void>::rethrow()
{
	if (!(this->m_exception == nullptr))
		std::rethrow_exception(this->m_exception);
}

}

#endif // LIBYB_ASYNC_TASK_RESULT_HPP
