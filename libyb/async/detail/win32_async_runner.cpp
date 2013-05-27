#include "../async_runner.hpp"
#include "../../utils/noncopyable.hpp"
#include "win32_wait_context.hpp"
#include <list>
#include <windows.h>
#include <stdexcept>

using namespace yb;
using namespace yb::detail;

struct async_promise_base::impl
{
	explicit impl(async_runner * runner)
		: m_refcount(0), m_runner(runner), m_applied_cancel(cl_none), m_requested_cancel(cl_none)
	{
		hFinishedEvent = CreateEvent(0, TRUE, FALSE, 0);
		if (!hFinishedEvent)
			throw std::runtime_error("failed to create finish event");
	}

	~impl()
	{
		CloseHandle(hFinishedEvent);
	}

	LONG m_refcount;
	async_runner * m_runner;
	cancel_level m_applied_cancel;
	cancel_level m_requested_cancel;
	HANDLE hFinishedEvent;
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
	InterlockedIncrement(&m_pimpl->m_refcount);
}

void async_promise_base::release()
{
	if (!InterlockedDecrement(&m_pimpl->m_refcount))
		delete this;
}

void async_promise_base::mark_finished()
{
	SetEvent(m_pimpl->hFinishedEvent);
}

void async_promise_base::wait()
{
	WaitForSingleObject(m_pimpl->hFinishedEvent, INFINITE);
}

void async_promise_base::perform_pending_cancels()
{
	if (m_pimpl->m_applied_cancel < m_pimpl->m_requested_cancel)
	{
		m_pimpl->m_applied_cancel = m_pimpl->m_requested_cancel;
		this->do_cancel(m_pimpl->m_applied_cancel);
	}
}

namespace {

struct cs_holder
	: noncopyable
{
	cs_holder(CRITICAL_SECTION & cs)
		: m_cs(cs)
	{
		EnterCriticalSection(&m_cs);
	}

	~cs_holder()
	{
		LeaveCriticalSection(&m_cs);
	}

	CRITICAL_SECTION & m_cs;
};

struct handle_holder
	: noncopyable
{
	explicit handle_holder(HANDLE handle = 0)
		: m_handle(handle)
	{
	}

	~handle_holder()
	{
		if (m_handle)
			CloseHandle(m_handle);
	}

	HANDLE get()
	{
		return m_handle;
	}

	void attach(HANDLE handle)
	{
		this->detach();
		m_handle = handle;
	}

	HANDLE detach()
	{
		HANDLE h = m_handle;
		m_handle = 0;
		return h;
	}

	HANDLE m_handle;
};

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

} // namespace

struct async_runner::impl
{
	impl()
		: hThread(0), stopped(false)
	{
		hQueueUpdated.attach(CreateEvent(0, FALSE, FALSE, 0));
		if (!hQueueUpdated.get())
			throw std::runtime_error("cannot create update event");

		InitializeCriticalSection(&queue_mutex);
	}

	~impl()
	{
		this->stop();
		DeleteCriticalSection(&queue_mutex);
	}

	void start()
	{
		assert(hThread == 0);

		DWORD dwThreadId;
		hThread = CreateThread(0, 0, &runner_thread, this, 0, &dwThreadId);
		if (!hThread)
			throw std::runtime_error("cannot create runner thread");
	}

	void stop()
	{
		if (hThread)
		{
			{
				cs_holder l(queue_mutex);
				stopped = true;
			}

			SetEvent(hQueueUpdated.get());
			WaitForSingleObject(hThread, INFINITE);
			CloseHandle(hThread);
			hThread = 0;
		}
	}

	void run()
	{
		task_wait_preparation_context wait_ctx;
		task_wait_preparation_context_impl & wait_ctx_impl = *wait_ctx.get();

		for (;;)
		{
			wait_ctx.clear();

			{
				cs_holder l(queue_mutex);
				if (stopped)
					break;

				for (std::list<parallel_promise>::iterator it = promises.begin(); it != promises.end(); ++it)
					it->promise->perform_pending_cancels();

				for (std::list<parallel_promise>::iterator it = promises.begin(); it != promises.end(); ++it)
				{
					assert(it->promise != 0);

					task_wait_memento_builder mb(wait_ctx);
					it->promise->prepare_wait(wait_ctx);
					it->m = mb.finish();
				}
			}

			wait_ctx_impl.m_handles.push_back(hQueueUpdated.get());

			if (wait_ctx_impl.m_finished_tasks)
			{
				task_wait_finalization_context finish_ctx;
				finish_ctx.finished_tasks = wait_ctx_impl.m_finished_tasks;
				this->finish_wait(finish_ctx);
			}
			else
			{
				DWORD dwRes = WaitForMultipleObjects(wait_ctx_impl.m_handles.size(), wait_ctx_impl.m_handles.data(), FALSE, INFINITE);
				assert(dwRes >= WAIT_OBJECT_0 && dwRes < WAIT_OBJECT_0 + wait_ctx_impl.m_handles.size());

				if (dwRes - WAIT_OBJECT_0 == wait_ctx_impl.m_handles.size() - 1)
					continue;

				task_wait_finalization_context finish_ctx;
				finish_ctx.finished_tasks = false;
				finish_ctx.selected_poll_item = dwRes - WAIT_OBJECT_0;
				this->finish_wait(finish_ctx);
			}
		}
	}

	void finish_wait(task_wait_finalization_context & ctx)
	{
		cs_holder l(queue_mutex);
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

	void run2()
	{
		try
		{
			this->run();
		}
		catch (...)
		{
		}

		for (std::list<parallel_promise>::iterator it = promises.begin(); it != promises.end(); ++it)
			it->promise->cancel_and_wait();
	}

	static DWORD CALLBACK runner_thread(LPVOID lpParam)
	{
		impl * pimpl = (impl *)lpParam;
		pimpl->run2();
		return 0;
	}

	CRITICAL_SECTION queue_mutex;
	std::list<parallel_promise> promises;

	HANDLE hThread;
	handle_holder hQueueUpdated;

	bool stopped;
};

async_runner::async_runner()
	: m_pimpl(new impl())
{
	m_pimpl->start();
}

async_runner::~async_runner()
{
}

async_runner::submit_context::submit_context(async_runner & runner)
	: m_runner(runner)
{
	EnterCriticalSection(&m_runner.m_pimpl->queue_mutex);
	m_runner.m_pimpl->promises.push_back(parallel_promise());
}

async_runner::submit_context::~submit_context()
{
	if (m_runner.m_pimpl->promises.back().promise == 0)
		m_runner.m_pimpl->promises.pop_back();
	LeaveCriticalSection(&m_runner.m_pimpl->queue_mutex);
}

void async_runner::submit_context::submit(detail::async_promise_base * p)
{
	assert(m_runner.m_pimpl->promises.back().promise == 0);
	m_runner.m_pimpl->promises.back().promise = p;
	p->addref();
	SetEvent(m_runner.m_pimpl->hQueueUpdated.get());
}

void async_promise_base::cancel(cancel_level cl)
{
	cs_holder l(m_pimpl->m_runner->m_pimpl->queue_mutex);
	m_pimpl->m_requested_cancel = cl;
	SetEvent(m_pimpl->m_runner->m_pimpl->hQueueUpdated.get());
}
