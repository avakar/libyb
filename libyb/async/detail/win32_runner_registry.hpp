#ifndef LIBYB_ASYNC_DETAIL_WIN32_RUNNER_REGISTRY_HPP
#define LIBYB_ASYNC_DETAIL_WIN32_RUNNER_REGISTRY_HPP

#include "runner_registry.hpp"
#include <windows.h>

namespace yb {

struct handle_completion_sink
{
	virtual void on_signaled(runner_registry & rr, HANDLE h) throw() = 0;
};

class runner_registry
{
public:
	virtual void schedule_work(runner_work & w) throw() = 0;

	virtual void add_handle(HANDLE h, handle_completion_sink & sink) = 0;
	virtual void remove_handle(HANDLE h, handle_completion_sink & sink) = 0;
};

};

#endif // LIBYB_ASYNC_DETAIL_WIN32_RUNNER_REGISTRY_HPP
