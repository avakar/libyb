#include "../group.hpp"
#include "parallel_compositor.hpp"
#include "../../utils/detail/pthread_mutex.hpp"
#include "../../utils/detail/linux_event.hpp"
#include "linux_wait_context.hpp"
#include <cassert>
#include <memory>
#include <list>
#include <atomic>
#include <sys/eventfd.h>
#include <unistd.h>
#include <poll.h>

struct yb::group::impl
{
public:
	impl()
		: m_refcount(1)
	{
	}

	void addref()
	{
		++m_refcount;
	}

	void release()
	{
		m_update_event.set();
		if (--m_refcount == 0)
			delete this;
	}

	void add_task(task<void> && t)
	{
		if (!t.has_task())
			return;

		yb::detail::scoped_pthread_lock l(m_mutex);
		m_ready_tasks.add_task(std::move(t));
		m_update_event.set();
	}

	class task_impl
		: public task_base<void>
	{
	public:
		explicit task_impl(impl * pimpl)
			: m_pimpl(pimpl)
		{
			m_pimpl->addref();
		}

		~task_impl()
		{
			m_pimpl->release();
		}

		task<void> cancel_and_wait() throw() override
		{
			m_pimpl->m_active_tasks.cancel_and_wait([](task<void> const &){});
			return yb::async::value();
		}

		void prepare_wait(task_wait_preparation_context & ctx, cancel_level cl) override
		{
			{
				yb::detail::scoped_pthread_lock l(m_pimpl->m_mutex);
				m_pimpl->m_active_tasks.append(std::move(m_pimpl->m_ready_tasks));
				m_pimpl->m_update_event.reset();
			}

			if (m_pimpl->m_active_tasks.empty() && m_pimpl->m_refcount.load(std::memory_order_acquire) == 1)
			{
				ctx.set_finished();
				m_nested_mem.clear();
			}
			else
			{
				ctx.get()->m_pollfds.push_back(m_pimpl->m_update_event.get_poll());

				yb::task_wait_memento_builder mb(ctx);
				m_pimpl->m_active_tasks.prepare_wait(ctx, cl);
				m_nested_mem = mb.finish();
			}
		}

		task<void> finish_wait(task_wait_finalization_context & ctx) throw() override
		{
			if (ctx.contains(m_nested_mem))
				m_pimpl->m_active_tasks.finish_wait(ctx, [](yb::task<void> const &) {});

			if (m_pimpl->m_active_tasks.empty() && m_pimpl->m_refcount.load(std::memory_order_acquire) == 1)
				return yb::async::value();

			return nulltask;
		}

	private:
		impl * m_pimpl;
		yb::task_wait_memento m_nested_mem;
	};

private:
	std::atomic<int> m_refcount;
	detail::parallel_compositor m_active_tasks;

	yb::detail::pthread_mutex m_mutex;

	detail::linux_event m_update_event;
	detail::parallel_compositor m_ready_tasks;
};

yb::group::group()
	: m_pimpl(nullptr)
{
}

yb::group::group(group && o)
	: m_pimpl(o.m_pimpl)
{
	o.m_pimpl = nullptr;
}

yb::group::~group()
{
	if (m_pimpl)
		m_pimpl->release();
}

yb::group & yb::group::operator=(group && o)
{
	group tmp(std::move(o));
	std::swap(m_pimpl, tmp.m_pimpl);
	return *this;
}

void yb::group::clear()
{
	if (m_pimpl)
		m_pimpl->release();
	m_pimpl = nullptr;
}

yb::task<void> yb::group::create(group & g)
{
	std::unique_ptr<impl> pimpl(new impl());
	yb::task<void> res(yb::task<void>::from_task(new impl::task_impl(pimpl.get())));
	if (g.m_pimpl)
		g.m_pimpl->release();
	g.m_pimpl = pimpl.release();
	return std::move(res);
}

void yb::group::post(task<void> && t)
{
	m_pimpl->add_task(std::move(t));
}
