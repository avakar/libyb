#ifndef LIBYB_ASYNC_DETAIL_DOUBLE_BUFFER_TASK_HPP
#define LIBYB_ASYNC_DETAIL_DOUBLE_BUFFER_TASK_HPP

#include "../../utils/noncopyable.hpp"
#include <utility>

namespace yb {
namespace detail {

template <typename R>
class double_buffer_task
	: public task_base<void>, noncopyable
{
public:
	explicit double_buffer_task(size_t count, std::function<task<R>(size_t)> const & create_fn, std::function<void(size_t, R const &)> const & collect_fn)
		: m_task_count(count), m_active_tasks(count), m_head(0), m_cl(cl_none), m_create_fn(create_fn), m_collect_fn(collect_fn)
	{
		m_tasks.reset(new item[count]);
		for (size_t i = 0; i < m_task_count; ++i)
			m_tasks[i].t = this->create_task(i);
		this->collect();
	}

	task<void> cancel_and_wait() throw() override
	{
		m_cl = cl_kill;
		for (size_t i = 0; i < m_active_tasks; ++i)
		{
			size_t idx = (m_head+i) % m_task_count;
			task<R> r = m_tasks[idx].t.cancel_and_wait();
			if (r.has_value())
			{
				try
				{
					m_collect_fn(idx, r.get());
				}
				catch (...)
				{
					if (m_exc == nullptr)
						m_exc = r.exception();
				}
			}
			else
			{
				if (m_exc == nullptr)
					m_exc = r.exception();
			}
		}

		return m_exc == nullptr? task<void>::from_value(): task<void>::from_exception(m_exc);
	}

	void prepare_wait(task_wait_preparation_context & ctx, cancel_level cl) override
	{
		m_cl = cl;
		for (size_t i = 0; i < m_active_tasks; ++i)
		{
			size_t idx = (m_head + i) % m_task_count;

			task_wait_memento_builder b(ctx);
			m_tasks[idx].t.prepare_wait(ctx, cl);
			m_tasks[idx].m = b.finish();
		}
	}

	task<void> finish_wait(task_wait_finalization_context & ctx) throw() override
	{
		for (size_t i = 0; i < m_active_tasks; ++i)
		{
			size_t idx = (m_head + i) % m_task_count;
			if (ctx.contains(m_tasks[idx].m))
				m_tasks[idx].t.finish_wait(ctx);
		}

		this->collect();

		return m_active_tasks? nulltask: m_exc == nullptr? async::value(): async::raise<void>(m_exc);
	}

private:
	void collect()
	{
		for (; m_active_tasks && m_tasks[m_head].t.has_result(); m_head = (m_head + 1) % m_task_count)
		{
			task<R> & t = m_tasks[m_head].t;

			task<R> r = t.get_result();
			if (r.has_value())
			{
				try
				{
					m_collect_fn(m_head, r.get());
				}
				catch (...)
				{
					--m_active_tasks;
					this->collect_exc(std::current_exception());
					continue;
				}

				if (m_cl < cl_quit)
				{
					t = this->create_task(m_head);
				}
				else
				{
					t.clear();
					--m_active_tasks;
				}
			}
			else
			{
				this->collect_exc(r.exception());
				t.clear();
				--m_active_tasks;
			}
		}
	}

	void collect_exc(std::exception_ptr && e)
	{
		if (m_exc == nullptr)
		{
			m_exc = std::move(e);
			this->cancel(cl_abort);
		}
	}

	task<R> create_task(size_t i)
	{
		try
		{
			return m_create_fn(i);
		}
		catch (...)
		{
			return async::raise<R>();
		}
	}

	struct item
	{
		task<R> t;
		task_wait_memento m;
	};

	std::unique_ptr<item[]> m_tasks;
	size_t m_task_count;
	size_t m_active_tasks;
	size_t m_head;
	cancel_level m_cl;
	std::function<task<R>(size_t)> m_create_fn;
	std::function<void(size_t, R const &)> m_collect_fn;
	std::exception_ptr m_exc;
};


} // namespace detail
} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_DOUBLE_BUFFER_TASK_HPP
