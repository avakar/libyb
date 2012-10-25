#ifndef LIBYB_ASYNC_CHANNEL_HPP
#define LIBYB_ASYNC_CHANNEL_HPP

#include "task.hpp"
#include <stdexcept>

namespace yb {

template <typename T>
class channel_core
{
public:
	channel_core()
		: m_refcount(1), m_has_value(false)
	{
	}

	void addref()
	{
		++m_refcount;
	}

	void release()
	{
		if (!--m_refcount)
			delete this;
	}

	bool empty() const
	{
		return !m_has_value;
	}

	void clear()
	{
		if (m_has_value)
		{
			this->as_result()->~task_result();
			m_has_value = false;
		}
	}

	template <typename U>
	void set(U && result)
	{
		assert(!m_has_value);
		m_has_value = true;
		new(&m_value) task_result<T>(std::forward<U>(result));
	}

	task_result<T> const & get() const
	{
		return *this->as_result();
	}

	task_result<T> & get()
	{
		return *this->as_result();
	}

private:
	~channel_core()
	{
		this->clear();
	}

	task_result<T> * as_result() { return reinterpret_cast<task_result<T> *>(&m_value); }

	int m_refcount;
	bool m_has_value;
	typename std::aligned_storage<sizeof(task_result<T>), std::alignment_of<task_result<T>>::value>::type m_value;
};

template <typename T>
class channel_receive_task
	: public task_base<T>
{
public:
	channel_receive_task(channel_core<T> * c)
		: m_core(c)
	{
		m_core->addref();
	}

	~channel_receive_task()
	{
		m_core->release();
	}

	void cancel() throw()
	{
		if (m_core)
		{
			m_core->release();
			m_core = 0;
		}
	}

	task_result<T> wait() throw()
	{
		assert(!m_core);
		return std::copy_exception(std::runtime_error("cancelled"));
	}

	void prepare_wait(task_wait_preparation_context & ctx)
	{
		if (!m_core || !m_core->empty())
			ctx.set_finished();
	}

	task<T> finish_wait(task_wait_finalization_context & ctx) throw()
	{
		if (!m_core)
		{
			return async::raise<T>(std::runtime_error("cancelled"));
		}
		else
		{
			assert(!m_core->empty());
			return task<T>(std::move(m_core->get()));
		}
	}

private:
	channel_core<T> * m_core;

	channel_receive_task(channel_receive_task const &);
	channel_receive_task & operator=(channel_receive_task const &);
};

template <typename T>
class channel_base
{
public:
	channel_base();
	~channel_base();
	channel_base(channel_base const & o);
	channel_base & operator=(channel_base const & o);

	task<T> receive();
	void send(task_result<T> && r);
	void send(task_result<T> const & r);

protected:
	channel_core<T> * m_core;
};

template <typename T>
class channel
	: public channel_base<T>
{
public:
	void send(T const & value);
	void send(T && value);
	using channel_base<T>::send;
};

template <>
class channel<void>
	: public channel_base<void>
{
public:
	void send();
	using channel_base<void>::send;
};

} // namespace yb


namespace yb {

template <typename T>
channel_base<T>::channel_base()
	: m_core(new channel_core<T>())
{
}

template <typename T>
channel_base<T>::~channel_base()
{
	m_core->release();
}

template <typename T>
channel_base<T>::channel_base(channel_base const & o)
	: m_core(o.m_core)
{
	m_core->addref();
}

template <typename T>
channel_base<T> & channel_base<T>::operator=(channel_base const & o)
{
	o.m_core->addref();
	m_core->release();
	m_core = o.m_core;
	return *this;
}

template <typename T>
task<T> channel_base<T>::receive()
{
	return protect([this] {
		return task<T>(new channel_receive_task<T>(m_core));
	});
}

template <typename T>
void channel_base<T>::send(task_result<T> && r)
{
	m_core->set(std::move(r));
}

template <typename T>
void channel_base<T>::send(task_result<T> const & r)
{
	m_core->set(r);
}

template <typename T>
void channel<T>::send(T const & value)
{
	m_core->set(value);
}

template <typename T>
void channel<T>::send(T && value)
{
	m_core->set(std::move(value));
}

inline void channel<void>::send()
{
	m_core->set(task_result<void>());
}

} // namespace yb

#endif // LIBYB_ASYNC_CHANNEL_HPP
