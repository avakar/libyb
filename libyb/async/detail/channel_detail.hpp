#ifndef LIBYB_ASYNC_DETAIL_CHANNEL_DETAIL_HPP
#define LIBYB_ASYNC_DETAIL_CHANNEL_DETAIL_HPP

#include "../../utils/detail/win32_mutex.hpp" // XXX
#include "../promise.hpp"

#include <list>
#include <deque>

namespace yb {
namespace detail {

template <typename T>
class unbounded_channel_buffer
	: public std::deque<T>
{
public:
	bool full() const
	{
		return false;
	}
};

template <typename T>
class null_channel_buffer
{
public:
	T & front()
	{
		throw std::runtime_error("XXX");
	}

	void push_back(T &&)
	{
		throw std::runtime_error("XXX");
	}

	void pop_front()
	{
	}

	bool empty() const
	{
		return true;
	}

	bool full() const
	{
		return true;
	}
};

template <typename T>
class channel_buffer_base
{
public:
	virtual ~channel_buffer_base() {}
	virtual yb::task<T> receive() = 0;
	virtual yb::task<void> send(yb::task<T> && t) = 0;
};

template <typename T, typename Buffer>
class channel_buffer
	: public channel_buffer_base<T>
{
public:
	~channel_buffer()
	{
		assert(m_readers.empty());
		assert(m_writers.empty());
	}

	yb::task<T> receive() override
	{
		yb::detail::scoped_win32_lock l(m_buffer_lock);
		if (!m_buffer.empty())
		{
			yb::task<T> res(std::move(m_buffer.front()));
			m_buffer.pop_front();
			this->update_writers();
			return std::move(res);
		}
		else if (!m_writers.empty())
		{
			writer * w = m_writers.front();
			w->it = m_writers.end();
			m_writers.pop_front();
			yb::task<T> res(std::move(w->value));
			w->promise.set();
			return std::move(res);
		}
		else
		{
			std::unique_ptr<reader> pr(new reader());
			m_readers.push_back(pr.get());
			pr.release();

			reader * r = m_readers.back();
			r->it = std::prev(m_readers.end());
			return r->promise.wait().continue_with([this, r](task<task<T>> t) {
				yb::detail::scoped_win32_lock l(m_buffer_lock);
				if (r->it != m_readers.end())
					m_readers.erase(r->it);
				delete r;
				return t.get();
			});
		}
	}

	yb::task<void> send(yb::task<T> && t) override
	{
		yb::detail::scoped_win32_lock l(m_buffer_lock);
		if (!m_readers.empty())
		{
			reader * r = m_readers.front();
			r->it = m_readers.end();
			m_readers.pop_front();

			r->promise.set(std::move(t));
			return yb::async::value();
		}
		else if (!m_buffer.full())
		{
			m_buffer.push_back(std::move(t));
			return yb::async::value();
		}
		else
		{
			std::unique_ptr<writer> pw(new writer());
			m_writers.push_back(pw.get());
			pw.release();

			writer * w = m_writers.back();
			w->it = std::prev(m_writers.end());
			w->value = std::move(t);

			return w->promise.wait().continue_with([this, w](task<void> t) {
				yb::detail::scoped_win32_lock l(m_buffer_lock);
				if (w->it != m_writers.end())
					m_writers.erase(w->it);
				delete w;
				return std::move(t);
			});
		}
	}

private:
	void update_writers()
	{
		while (!m_buffer.full() && !m_writers.empty())
		{
			writer * w = m_writers.front();
			m_buffer.push_back(std::move(w->value));
			m_writers.erase(w->it);
			w->it = m_writers.end();
			w->promise.set();
		}
	}

	yb::detail::win32_mutex m_buffer_lock;
	Buffer m_buffer;

	struct reader
	{
		promise<task<T>> promise;
		typename std::list<reader *>::iterator it;
	};

	struct writer
	{
		task<T> value;
		promise<void> promise;
		typename std::list<writer *>::iterator it;
	};

	std::list<reader *> m_readers;
	std::list<writer *> m_writers;
};

} // namespace detail
} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_CHANNEL_DETAIL_HPP
