#ifndef LIBYB_ASYNC_DETAIL_CHANNEL_DETAIL_HPP
#define LIBYB_ASYNC_DETAIL_CHANNEL_DETAIL_HPP

#include "../utils/detail/win32_mutex.hpp" // XXX
#include <windows.h>
#include "detail/win32_handle_task.hpp"
#include "detail/notification_event.hpp"
#include "detail/semaphore.hpp"

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
class channel_buffer_base
{
public:
	virtual yb::task<T> receive() = 0;
	virtual yb::task<void> send(yb::task<T> && t) = 0;
};

template <typename T, typename Buffer>
class channel_buffer
	: public channel_buffer_base<T>
{
public:
	yb::task<T> receive() override
	{
		return m_read_sem.wait().then([this]() {
			yb::detail::scoped_win32_lock l(m_buffer_lock);
			if (!m_buffer.empty())
			{
				yb::task<T> res = std::move(m_buffer.front());
				m_buffer.pop_front();

				while (!m_writers.empty() && !m_buffer.full())
				{
					writer * w = m_writers.front();
					m_buffer.push_back(std::move(w->m_value));
					m_writers.pop_front();
					w->it = m_writers.end();
					w->ev.set();
				}

				return std::move(res);
			}
			else
			{
				assert(!m_writers.empty());
				writer * w = m_writers.front();
				yb::task<T> res = std::move(w->m_value);
				m_writers.pop_front();
				w->it = m_writers.end();
				w->ev.set();
				return std::move(res);
			}
		});
	}

	yb::task<void> send(yb::task<T> && t) override
	{
		yb::detail::scoped_win32_lock l(m_buffer_lock);

		if (!m_buffer.full())
		{
			m_buffer.push_back(std::move(t));
			m_read_sem.set();
			return yb::async::value();
		}
		else
		{
			std::unique_ptr<writer> p(new writer(std::move(t)));
			m_writers.push_back(p.get());
			writer * w = p.release();
			w->it = std::prev(m_writers.end());
			m_read_sem.set();
			return w->ev.wait().follow_with([this, w]() {
				yb::detail::scoped_win32_lock l(m_buffer_lock);
				if (w->it != m_writers.end())
					m_writers.erase(w->it);
				delete w;
			});
		}
	}

private:
	semaphore m_read_sem;
	yb::detail::win32_mutex m_buffer_lock;
	Buffer m_buffer;

	struct writer
	{
		yb::notification_event ev;
		yb::task<T> m_value;
		typename std::list<writer *>::iterator it;

		explicit writer(yb::task<T> && value)
			: m_value(std::move(value))
		{
		}
	};

	std::list<writer *> m_writers;
};

} // namespace detail
} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_CHANNEL_DETAIL_HPP
