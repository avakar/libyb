#ifndef LIBYB_ASYNC_DETAIL_CHANNEL_DETAIL_HPP
#define LIBYB_ASYNC_DETAIL_CHANNEL_DETAIL_HPP

#include "circular_buffer.hpp"

namespace yb {

template <typename T, size_t Capacity>
class channel_send_task
	: public task_base<void>
{
public:
	typedef shared_circular_buffer<task_result<T>, Capacity> buffer_type;

	channel_send_task(buffer_type * buffer, task_result<T> && r)
		: m_buffer(buffer), m_value(std::move(r))
	{
		m_buffer->addref();
	}

	~channel_send_task()
	{
		if (m_buffer)
			m_buffer->release();
	}

	void cancel(cancel_level cl) throw()
	{
		if (cl >= cl_abort && m_buffer)
		{
			m_buffer->release();
			m_buffer = 0;
		}
	}

	task_result<void> cancel_and_wait() throw()
	{
		this->cancel(cl_kill);
		return task_result<void>();
	}

	void prepare_wait(task_wait_preparation_context & ctx)
	{
		if (m_buffer && m_buffer->full())
			return;

		if (m_buffer)
			m_buffer->push_back(std::move(m_value));

		ctx.set_finished();
	}

	task<void> finish_wait(task_wait_finalization_context &) throw()
	{
		if (m_buffer)
			return async::value();
		else
			return async::raise<void>(std::runtime_error("cancelled"));
	}

private:
	buffer_type * m_buffer;
	task_result<T> m_value;
};

template <typename T, size_t Capacity>
class channel_receive_task
	: public task_base<T>
{
public:
	typedef shared_circular_buffer<task_result<T>, Capacity> buffer_type;

	explicit channel_receive_task(buffer_type * c)
		: m_buffer(c)
	{
		m_buffer->addref();
	}

	~channel_receive_task()
	{
		if (m_buffer)
			m_buffer->release();
	}

	void cancel(cancel_level cl) throw()
	{
		if (cl >= cl_abort && m_buffer)
		{
			m_buffer->release();
			m_buffer = 0;
		}
	}

	task_result<T> cancel_and_wait() throw()
	{
		this->cancel(cl_kill);
		assert(!m_buffer);
		return std::copy_exception(std::runtime_error("cancelled"));
	}

	void prepare_wait(task_wait_preparation_context & ctx)
	{
		if (m_buffer && m_buffer->empty())
			return;

		if (!m_buffer)
		{
			m_result = async::raise<T>(std::runtime_error("cancelled"));
		}
		else
		{
			m_result = async::result(std::move(m_buffer->front()));
			m_buffer->pop_front();
		}

		ctx.set_finished();
	}

	task<T> finish_wait(task_wait_finalization_context &) throw()
	{
		assert(!m_result.empty());
		return std::move(m_result);
	}

private:
	buffer_type * m_buffer;
	task<T> m_result;

	channel_receive_task(channel_receive_task const &);
	channel_receive_task & operator=(channel_receive_task const &);
};

} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_CHANNEL_DETAIL_HPP
