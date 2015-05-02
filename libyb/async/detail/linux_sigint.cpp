#include "../sigint.hpp"
#include "../channel.hpp"
#include "linux_fdpoll_task.hpp"
#include "../../utils/detail/unix_system_error.hpp"
#include <pthread.h>
#include <signal.h>
#include <sys/eventfd.h>
#include <unistd.h>

static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_event = -1;
static int g_refcount = 0;
static struct sigaction g_prev_action;

static void handler(int)
{
	uint64_t val = 1;
	write(g_event, &val, sizeof val);
}

yb::task<void> yb::wait_for_sigint()
{
	pthread_mutex_lock(&g_mutex);
	if (g_refcount == 0)
	{
		g_event = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
		if (g_event == -1)
		{
			int e = errno;
			pthread_mutex_unlock(&g_mutex);
			return yb::async::raise<void>(yb::detail::unix_system_error(e));
		}

		struct sigaction sa = {};
		sa.sa_handler = &handler;
		sigaction(SIGINT, &sa, &g_prev_action);
	}

	++g_refcount;
	pthread_mutex_unlock(&g_mutex);

	return yb::loop2<short>([](task<short> & s, cancel_level cl) -> task<void> {
		while (cl < cl_abort)
		{
			uint64_t val;
			int r = read(g_event, &val, sizeof val);
			if (r != -1)
				return async::value();
			int e = errno;
			if (e == EINTR)
				continue;
			if (e == EAGAIN || e == EWOULDBLOCK)
			{
				s = make_linux_pollfd_task(g_event, POLLIN, [](cancel_level cl) { return cl < cl_abort; });
				return yb::nulltask;
			}
			return async::raise<void>(detail::unix_system_error(e));
		}

		return async::raise<void>(task_cancelled(cl));
	}).continue_with([](task<void> && t) {
		pthread_mutex_lock(&g_mutex);
		if (--g_refcount == 0)
		{
			sigaction(SIGINT, &g_prev_action, 0);
			close(g_event);
		}
		pthread_mutex_unlock(&g_mutex);
		return std::move(t);
	});
}
