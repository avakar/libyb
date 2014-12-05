#ifndef LIBYB_ASYNC_DETAIL_WIN32_RUNNER_REGISTRY_HPP
#define LIBYB_ASYNC_DETAIL_WIN32_RUNNER_REGISTRY_HPP

#include <windows.h>

namespace yb {

struct runner_registry;

struct handle_completion_sink
{
	virtual void on_signaled(runner_registry & rr, HANDLE h) throw() = 0;
};

struct runner_posted_routine
{
	virtual void run() throw() = 0;
};

struct runner_registry
{
	virtual void post(runner_posted_routine & sink) throw() = 0;
	virtual void add_handle(HANDLE h, handle_completion_sink & sink) = 0;
	virtual void remove_handle(HANDLE h, handle_completion_sink & sink) = 0;
};

};

#endif // LIBYB_ASYNC_DETAIL_WIN32_RUNNER_REGISTRY_HPP
