#ifndef LIBYB_ASYNC_DETAIL_PREPARED_TASK_HPP
#define LIBYB_ASYNC_DETAIL_PREPARED_TASK_HPP

#include "../task.hpp"
#include <memory>

namespace yb {
namespace detail {

class prepared_task;

struct prepared_task_event_sink
{
	virtual prepared_task * schedule_update(prepared_task * pt) throw() = 0;
};

class prepared_task
{
public:
	prepared_task * m_next;

	prepared_task();

	void addref() throw();
	void release() throw();

	// Shadow task interface
	void schedule_cancel(cancel_level cl) throw();

	// Runner interface
	virtual void start(runner_registry & rr, prepared_task_completion_sink & sink) throw() = 0;
	virtual void cancel(runner_registry * rr, cancel_level cl) throw() = 0;

	virtual prepared_task * update() = 0;

	void attach_event_sink(prepared_task_event_sink & r) throw();
	void detach_event_sink() throw();

protected:
	~prepared_task();

private:
	struct impl;
	std::unique_ptr<impl> m_pimpl;

	prepared_task(prepared_task const &);
	prepared_task & operator=(prepared_task const &);
};

template <typename R>
class prepared_task_impl;

} // namespace detail
} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_PREPARED_TASK_HPP
