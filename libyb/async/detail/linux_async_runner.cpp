#include "../async_runner.hpp"
#include "../../utils/noncopyable.hpp"
#include <stdexcept>
#include <pthread.h>
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

void async_promise_base::cancel(cancel_level cl)
{
	// XXX: submit a cancel
}

void async_promise_base::perform_pending_cancels()
{
	// XXX
}

struct async_runner::impl
{
};

async_runner::async_runner()
	: m_pimpl(new impl())
{
	// XXX: start the dispatch thread
}

async_runner::~async_runner()
{
	// XXX: cancel all scheduled tasks and stop the dispatch thread
}

async_runner::submit_context::submit_context(async_runner & runner)
	: m_runner(runner)
{
}

async_runner::submit_context::~submit_context()
{
}

void async_runner::submit_context::submit(detail::async_promise_base * p)
{
	// XXX: submit the task
}
