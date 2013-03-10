#ifndef LIBYB_ASYNC_ASYNC_CHANNEL_HPP
#define LIBYB_ASYNC_ASYNC_CHANNEL_HPP

#include "../utils/noncopyable.hpp"
#include "../vector_ref.hpp"
#include "task.hpp"
#include <vector>
#include <memory>

namespace yb {

namespace detail {

class async_channel_base
	: noncopyable
{
public:
	async_channel_base();
	~async_channel_base();

	class scoped_lock
		: noncopyable
	{
	public:
		explicit scoped_lock(async_channel_base * ch);
		~scoped_lock();

	private:
		async_channel_base * m_ch;
	};

	task<void> wait();
	void set();
	void reset();
	bool empty() const;

private:
	struct impl;
	std::unique_ptr<impl> m_pimpl;
};

} // namespace detail

template <typename T>
class async_channel
	: private detail::async_channel_base
{
public:
	task<void> receive(std::vector<T> & data)
	{
		return this->wait().follow_with([this, &data]() {
			scoped_lock l(this);
			data.swap(m_data);
			m_data.clear();
			this->reset();
		});
	}

	void send(T && r)
	{
		scoped_lock l(this);
		m_data.push_back(std::move(r));
		this->set();
	}

	void send(T const & r)
	{
		scoped_lock l(this);
		m_data.push_back(r);
		this->set();
	}

	void send(yb::vector_ref<T> const & v)
	{
		scoped_lock l(this);
		m_data.insert(m_data.end(), v.begin(), v.end());
		this->set();
	}

	void clear()
	{
		scoped_lock l(this);
		m_data.clear();
		this->reset();
	}

	using async_channel_base::empty;

private:
	std::vector<T> m_data;
};

} // namespace yb

#endif // LIBYB_ASYNC_ASYNC_CHANNEL_HPP
