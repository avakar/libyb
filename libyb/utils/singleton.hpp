#ifndef LIBYB_UTILS_SINGLETON_HPP
#define LIBYB_UTILS_SINGLETON_HPP

#include "atomic.hpp"
#include <type_traits>

namespace yb {

class singleton_impl
{
public:
	singleton_impl();
	~singleton_impl();

	void addref();
	void release();

protected:
	virtual void do_construct() = 0;
	virtual void do_destruct() = 0;

private:
	yb::atomic_uint m_lock;
	yb::atomic_uint m_refcount;
};

template <typename T>
class singleton_ptr;

template <typename T>
class singleton
	: private singleton_impl
{
private:
	void do_construct() override;
	void do_destruct() override;

	friend class singleton_ptr<T>;
	T * get();

	std::aligned_storage<sizeof(T), std::alignment_of<T>::value> m_storage;
};

template <typename T>
class singleton_ptr
{
public:
	singleton_ptr();
	explicit singleton_ptr(singleton<T> & s);
	singleton_ptr(singleton_ptr const &);
	singleton_ptr & operator=(singleton_ptr const &);
	singleton_ptr(singleton_ptr && o);
	singleton_ptr & operator=(singleton_ptr && o);
	~singleton_ptr();

	void reset();
	void reset(singleton<T> & s);

	T * operator->() const;

private:
	singleton<T> * m_s;
};

template <typename T>
singleton_ptr<T>::singleton_ptr()
	: m_s(nullptr)
{
}

template <typename T>
singleton_ptr<T>::singleton_ptr(singleton<T> & s)
	: m_s(nullptr)
{
	this->reset(s);
}

template <typename T>
singleton_ptr<T>::singleton_ptr(singleton_ptr && o)
	: m_s(o.m_s)
{
	o.m_s = nullptr;
}

template <typename T>
singleton_ptr<T>::singleton_ptr(singleton_ptr const & o)
	: m_s(nullptr)
{
	if (o.m_s)
		this->reset(*o.m_s);
}

template <typename T>
singleton_ptr<T> & singleton_ptr<T>::operator=(singleton_ptr const & o)
{
	if (o.m_s)
		this->reset(*o.m_s);
	else
		this->reset();
	return *this;
}

template <typename T>
singleton_ptr<T> & singleton_ptr<T>::operator=(singleton_ptr && o)
{
	std::swap(m_s, o.m_s);
	return *this;
}

template <typename T>
singleton_ptr<T>::~singleton_ptr()
{
	this->reset();
}

template <typename T>
void singleton_ptr<T>::reset()
{
	if (m_s)
	{
		m_s->release();
		m_s = 0;
	}
}

template <typename T>
void singleton_ptr<T>::reset(singleton<T> & s)
{
	s.addref();
	if (m_s)
		m_s->release();
	m_s = &s;
}

template <typename T>
T * singleton_ptr<T>::operator->() const
{
	return reinterpret_cast<T *>(&m_s->m_storage);
}

template <typename T>
T * singleton<T>::get()
{
	return reinterpret_cast<T *>(&m_storage);
}

template <typename T>
void singleton<T>::do_construct()
{
	new(&m_storage) T();
}

template <typename T>
void singleton<T>::do_destruct()
{
	this->get()->~T();
}

} // namespace yb

#endif // LIBYB_UTILS_SINGLETON_HPP
