#ifndef LIBYB_ASYNC_FUTURE_HPP
#define LIBYB_ASYNC_FUTURE_HPP

#include "detail/circular_buffer.hpp"
#include "task.hpp"
#include <stdexcept>

namespace yb {

template <typename T>
class promise
{
public:
	promise(promise const & o)
		: m_buffer(o.m_buffer)
	{
		m_buffer->addref();
	}

	~promise()
	{
		m_buffer->release();
	}

	promise & operator=(promise const & o)
	{
		o.m_buffer->addref();
		m_buffer->release();
		m_buffer = o.m_buffer;
		return *this;
	}

	void set_value(T && t) const
	{
		m_buffer->push_back(task_result<T>(std::move(t)));
	}

	void set_value(T const & t) const
	{
		m_buffer->push_back(task_result<T>(t));
	}

	task<T> get() const
	{
		return protect([this] {
			return task<T>(new task_impl(m_buffer));
		});
	}

	static promise create()
	{
		return promise(new shared_circular_buffer<task_result<T>, 1>());
	}

private:
	explicit promise(shared_circular_buffer<task_result<T>, 1> * buffer)
		: m_buffer(buffer)
	{
	}

	class task_impl
		: public task_base<T>
	{
	public:
		explicit task_impl(shared_circular_buffer<task_result<T>, 1> * buffer)
			: m_buffer(buffer)
		{
			m_buffer->addref();
		}

		~task_impl()
		{
			if (m_buffer)
				m_buffer->release();
		}

		void cancel(cancel_level_t) throw()
		{
			if (m_buffer && m_buffer->empty())
			{
				m_buffer->release();
				m_buffer = 0;
			}
		}

		task_result<T> cancel_and_wait() throw()
		{
			this->cancel(cancel_level_hard);

			if (m_buffer)
				return task_result<T>(m_buffer->front());
			else
				return task_result<T>(std::copy_exception(std::runtime_error("cancelled")));
		}

		void prepare_wait(task_wait_preparation_context & ctx)
		{
			if (!m_buffer || !m_buffer->empty())
				ctx.set_finished();
		}

		task<T> finish_wait(task_wait_finalization_context &) throw()
		{
			if (m_buffer)
				return async::result(m_buffer->front());
			else
				return async::raise<T>(std::runtime_error("cancelled"));
		}

	private:
		shared_circular_buffer<task_result<T>, 1> * m_buffer;

		task_impl(task_impl const &);
		task_impl & operator=(task_impl const &);
	};

	shared_circular_buffer<task_result<T>, 1> * m_buffer;
};

} // namespace yb

#endif // LIBYB_ASYNC_FUTURE_HPP
