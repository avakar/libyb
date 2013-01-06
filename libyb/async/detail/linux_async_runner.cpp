#include "../async_runner.hpp"
#include "linux_wait_context.hpp"
#include "../../utils/noncopyable.hpp"
#include <list>
#include <stdexcept>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/eventfd.h>
using namespace yb;
using namespace yb::detail;

struct async_promise_base::impl
	: noncopyable
{
	explicit impl(async_runner * runner)
		: m_runner(runner), m_refcount(1), m_finished(false)
	{
		if (pthread_mutex_init(&m_mutex, 0) != 0)
			throw std::runtime_error("cannot create mutex");

		if (pthread_cond_init(&m_cond, 0) != 0)
		{
			pthread_mutex_destroy(&m_mutex);
			throw std::runtime_error("cannot create condvar");
		}
	}

	~impl()
	{
		pthread_cond_destroy(&m_cond);
		pthread_mutex_destroy(&m_mutex);
	}

	async_runner * m_runner;
	int m_refcount;

	pthread_mutex_t m_mutex;
	pthread_cond_t m_cond;
	bool m_finished;

	cancel_level m_request_cl;
	cancel_level m_applied_cl;
};

async_promise_base::async_promise_base(async_runner * runner)
	: m_pimpl(new impl(runner))
{
}

async_promise_base::~async_promise_base()
{
}

void async_promise_base::addref()
{
	__sync_add_and_fetch(&m_pimpl->m_refcount, 1);
}

void async_promise_base::release()
{
	if (__sync_sub_and_fetch(&m_pimpl->m_refcount, 1) == 0)
		delete this;
}

void async_promise_base::mark_finished()
{
	pthread_mutex_lock(&m_pimpl->m_mutex);
	m_pimpl->m_finished = true;
	pthread_cond_broadcast(&m_pimpl->m_cond);
	pthread_mutex_unlock(&m_pimpl->m_mutex);
}

void async_promise_base::wait()
{
	pthread_mutex_lock(&m_pimpl->m_mutex);
	while (!m_pimpl->m_finished)
		pthread_cond_wait(&m_pimpl->m_cond, &m_pimpl->m_mutex);
	pthread_mutex_unlock(&m_pimpl->m_mutex);
}

void async_promise_base::perform_pending_cancels()
{
	if (m_pimpl->m_request_cl > m_pimpl->m_applied_cl)
	{
		this->do_cancel(m_pimpl->m_request_cl);
		m_pimpl->m_applied_cl = m_pimpl->m_request_cl;
	}
}

namespace {

struct parallel_promise
{
	async_promise_base * promise;
	task_wait_memento m;

	parallel_promise()
		: promise(0)
	{
	}

	~parallel_promise()
	{
		if (promise)
			promise->release();
	}

	parallel_promise(parallel_promise && o)
		: promise(o.promise)
	{
		o.promise = 0;
	}
};

struct scoped_mutex
{
	explicit scoped_mutex(pthread_mutex_t & mutex)
		: m_mutex(&mutex)
	{
		pthread_mutex_lock(m_mutex);
	}

	~scoped_mutex()
	{
		if (m_mutex)
			pthread_mutex_unlock(m_mutex);
	}

	void detach()
	{
		m_mutex = 0;
	}

	pthread_mutex_t * m_mutex;
};

} // namespace

struct async_runner::impl
{
	impl()
		: stopped(false)
	{
		if (pthread_mutex_init(&mutex, 0) != 0)
			throw std::runtime_error("failed to create a mutex");

		control_event = eventfd(0, 0);
		if (control_event == -1)
		{
			pthread_mutex_destroy(&mutex);
			throw std::runtime_error("failed to create eventfd");
		}

		if (fcntl(control_event, F_SETFL, O_NONBLOCK) == -1)
		{
			close(control_event);
			pthread_mutex_destroy(&mutex);
			throw std::runtime_error("failed to set O_NONBLOCK on an eventfd");
		}
	}

	~impl()
	{
		close(control_event);
		pthread_mutex_destroy(&mutex);
	}

	void run()
	{
		task_wait_preparation_context wait_ctx;
		task_wait_preparation_context_impl & wait_ctx_impl = *wait_ctx.get();

		while (!__atomic_load_n(&stopped, __ATOMIC_ACQUIRE))
		{
			wait_ctx.clear();

			for (std::list<parallel_promise>::iterator it = promises.begin(); it != promises.end(); ++it)
				it->promise->perform_pending_cancels();

			for (std::list<parallel_promise>::iterator it = promises.begin(); it != promises.end(); ++it)
			{
				assert(it->promise != 0);

				task_wait_memento_builder mb(wait_ctx);
				it->promise->prepare_wait(wait_ctx);
				it->m = mb.finish();
			}

			if (wait_ctx_impl.m_finished_tasks)
			{
				task_wait_finalization_context finish_ctx;
				finish_ctx.prep_ctx = &wait_ctx;
				finish_ctx.finished_tasks = wait_ctx_impl.m_finished_tasks;
				this->finish_wait(finish_ctx);
			}
			else
			{
				struct pollfd pfd = {};
				pfd.fd = control_event;
				pfd.events = POLLIN;
				wait_ctx_impl.m_pollfds.push_back(pfd);

				int r = poll(wait_ctx_impl.m_pollfds.data(), wait_ctx_impl.m_pollfds.size(), -1);
				assert(r > 0);

				if (wait_ctx_impl.m_pollfds.back().revents & POLLIN)
				{
					uint64_t val;
					int r = read(control_event, &val, sizeof val);
					assert(r >= 0);

					scoped_mutex l(mutex);
					promises.splice(promises.end(), new_promises);
					continue;
				}

				for (size_t i = 0; i < wait_ctx_impl.m_pollfds.size() - 1; ++i)
				{
					if (wait_ctx_impl.m_pollfds[i].revents)
					{
						task_wait_finalization_context finish_ctx;
						finish_ctx.prep_ctx = &wait_ctx;
						finish_ctx.finished_tasks = false;
						finish_ctx.selected_poll_item = i;
						this->finish_wait(finish_ctx);
						break;
					}
				}
			}
		}
	}

	void finish_wait(task_wait_finalization_context & ctx)
	{
		for (std::list<parallel_promise>::iterator it = promises.begin(); it != promises.end(); )
		{
			if (ctx.contains(it->m) && it->promise->finish_wait(ctx))
			{
				it->promise->mark_finished();
				it = promises.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	static void * dispatch_thread(void * ctx)
	{
		impl * pimpl = (impl *)ctx;

		try
		{
			pimpl->run();
		}
		catch (...)
		{
		}

		return 0;
	}

	void signal_control_event()
	{
		uint64_t val = 1;
		int r = write(control_event, &val, sizeof val);
		assert(r >= 0 || errno == EAGAIN);
	}

	pthread_mutex_t mutex;
	std::list<parallel_promise> promises;
	std::list<parallel_promise> new_promises;
	bool stopped;

	pthread_t thread;
	int control_event;
};

async_runner::async_runner()
	: m_pimpl(new impl())
{
	if (pthread_create(&m_pimpl->thread, 0, &impl::dispatch_thread, m_pimpl.get()) != 0)
		throw std::runtime_error("failed to create a dispatch thread");
}

async_runner::~async_runner()
{
	__atomic_store_n(&m_pimpl->stopped, true, __ATOMIC_RELEASE);
	m_pimpl->signal_control_event();

	void * retval;
	pthread_join(m_pimpl->thread, &retval);
}

async_runner::submit_context::submit_context(async_runner & runner)
	: m_runner(runner)
{
	scoped_mutex l(m_runner.m_pimpl->mutex);

	parallel_promise pp = {};
	m_runner.m_pimpl->new_promises.push_back(std::move(pp));

	l.detach();
}

async_runner::submit_context::~submit_context()
{
	if (m_runner.m_pimpl->new_promises.back().promise == 0)
		m_runner.m_pimpl->new_promises.pop_back();
	pthread_mutex_unlock(&m_runner.m_pimpl->mutex);
}

void async_runner::submit_context::submit(detail::async_promise_base * p)
{
	assert(m_runner.m_pimpl->new_promises.back().promise == 0);
	m_runner.m_pimpl->new_promises.back().promise = p;
	m_runner.m_pimpl->signal_control_event();
}

void async_promise_base::cancel(cancel_level cl)
{
	pthread_mutex_lock(&m_pimpl->m_runner->m_pimpl->mutex);
	if (m_pimpl->m_request_cl < cl)
		m_pimpl->m_request_cl = cl;
	m_pimpl->m_runner->m_pimpl->signal_control_event();
	pthread_mutex_unlock(&m_pimpl->m_runner->m_pimpl->mutex);
}
