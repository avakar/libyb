#include "async_runner.hpp"
#include "../utils/noncopyable.hpp"
#include "detail/win32_wait_context.hpp"
#include <list>
#include <windows.h>
using namespace yb;
using namespace yb::detail;

struct async_promise_base::impl
{
	impl()
	{
		hFinishedEvent = CreateEvent(0, TRUE, FALSE, 0);
		if (!hFinishedEvent)
			throw std::runtime_error("failed to create finish event");
	}

	~impl()
	{
		CloseHandle(hFinishedEvent);
	}

	HANDLE hFinishedEvent;
};

async_promise_base::async_promise_base()
	: m_pimpl(new impl())
{
}

async_promise_base::~async_promise_base()
{
}

void async_promise_base::mark_finished()
{
	SetEvent(m_pimpl->hFinishedEvent);
}

void async_promise_base::wait()
{
	WaitForSingleObject(m_pimpl->hFinishedEvent, INFINITE);
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

struct async_runner::impl
{
	impl()
		: hThread(0)
	{
		hStopEvent.attach(CreateEvent(0, TRUE, FALSE, 0));
		if (!hStopEvent.get())
			throw std::runtime_error("cannot create stop event");

		hQueueUpdated.attach(CreateEvent(0, TRUE, FALSE, 0));
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
			SetEvent(hStopEvent.get());
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
			wait_ctx_impl.m_handles.push_back(hStopEvent.get());
			parallel_tasks.prepare_wait(wait_ctx);
			wait_ctx_impl.m_handles.push_back(hQueueUpdated.get());

			if (wait_ctx_impl.m_finished_tasks)
			{
				task_wait_finalization_context finish_ctx;
				finish_ctx.finished_tasks = wait_ctx_impl.m_finished_tasks;
				parallel_tasks.finish_wait(finish_ctx);
			}
			else
			{
				DWORD dwRes = WaitForMultipleObjects(wait_ctx_impl.m_handles.size(), wait_ctx_impl.m_handles.data(), FALSE, INFINITE);
				assert(dwRes >= WAIT_OBJECT_0 && dwRes < WAIT_OBJECT_0 + wait_ctx_impl.m_handles.size());

				if (dwRes == WAIT_OBJECT_0)
					break;

				if (dwRes - WAIT_OBJECT_0 == wait_ctx_impl.m_handles.size() - 1)
				{
					cs_holder l(queue_mutex);
					for (size_t i = 0; i < queued_tasks.size(); ++i)
						parallel_tasks |= std::move(queued_tasks[i]);
					queued_tasks.clear();
					ResetEvent(hQueueUpdated.get());
					continue;
				}

				task_wait_finalization_context finish_ctx;
				finish_ctx.finished_tasks = false;
				finish_ctx.selected_poll_item = dwRes - WAIT_OBJECT_0;
				parallel_tasks.finish_wait(finish_ctx);
			}
		}
	}

	static DWORD CALLBACK runner_thread(LPVOID lpParam)
	{
		impl * pimpl = (impl *)lpParam;

		// FIXME: exception safety
		try
		{
			pimpl->run();
		}
		catch (...)
		{
			return 1;
		}

		return 0;
	}

	task<void> parallel_tasks;

	HANDLE hThread;
	handle_holder hStopEvent;
	handle_holder hQueueUpdated;

	CRITICAL_SECTION queue_mutex;
	std::vector<task<void>> queued_tasks;
};

async_runner::async_runner()
	: m_pimpl(new impl())
{
}

async_runner::~async_runner()
{
}

void async_runner::start()
{
	m_pimpl->start();
}

void async_runner::submit(task<void> && t)
{
	cs_holder l(m_pimpl->queue_mutex);
	m_pimpl->queued_tasks.push_back(std::move(t));
	SetEvent(m_pimpl->hQueueUpdated.get());
}
