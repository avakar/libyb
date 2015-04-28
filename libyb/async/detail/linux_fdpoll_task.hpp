#ifndef LIBYB_ASYNC_DETAIL_LINUX_FDPOLL_TASK_HPP
#define LIBYB_ASYNC_DETAIL_LINUX_FDPOLL_TASK_HPP

#include "../task_base.hpp"
#include "../cancel_exception.hpp"
#include "linux_wait_context.hpp"
#include "../../utils/noncopyable.hpp"
#include <exception>
#include <utility>
#include <poll.h>
#include <cassert>

namespace yb {
namespace detail {

template <typename Canceller>
class linux_fdpoll_task
	: public task_base<short>, noncopyable
{
public:
	linux_fdpoll_task(int fd, short events, Canceller && canceller)
		: m_fd(fd), m_events(events), m_canceller(std::move(canceller))
	{
	}

	task<short> cancel_and_wait() throw() override
	{
		if (m_fd != -1 && !m_canceller(cl_kill))
			m_fd = -1;

		if (m_fd != -1)
		{
			struct pollfd pf;
			pf.fd = m_fd;
			pf.events = m_events;
			pf.revents = 0;

			int r = poll(&pf, 1, -1);
			assert(r == 1); // TODO: under what circumstances can poll fail?

			return task<short>::from_value(pf.revents);
		}
		else
		{
			return task<short>::from_exception(std::copy_exception(task_cancelled()));
		}
	}

	void prepare_wait(task_wait_preparation_context & ctx, cancel_level cl) override
	{
		if (m_fd != -1 && !m_canceller(cl))
			m_fd = -1;

		if (m_fd == -1)
		{
			ctx.set_finished();
		}
		else
		{
			struct pollfd pf = {};
			pf.fd = m_fd;
			pf.events = m_events;
			ctx.get()->m_pollfds.push_back(pf);
		}
	}

	task<short> finish_wait(task_wait_finalization_context & ctx) throw() override
	{
		if (m_fd != -1)
			return async::value(ctx.prep_ctx->get()->m_pollfds[ctx.selected_poll_item].revents);
		else
			return async::raise<short>(task_cancelled());
	}

private:
	int m_fd;
	short m_events;
	Canceller m_canceller;
};

} // namespace detail

template <typename Canceller>
task<short> make_linux_pollfd_task(int fd, short events, Canceller && canceller)
{
	assert(fd >= 0);

	try
	{
		return task<short>::from_task(new detail::linux_fdpoll_task<Canceller>(fd, events, std::move(canceller)));
	}
	catch (...)
	{
		if (canceller(cl_kill))
		{
			struct pollfd pf = {};
			pf.fd = fd;
			pf.events = events;
			poll(&pf, 1, -1);
			return async::value(pf.revents);
		}

		return async::raise<short>(std::current_exception());
	}
}

} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_LINUX_FDPOLL_TASK_HPP
