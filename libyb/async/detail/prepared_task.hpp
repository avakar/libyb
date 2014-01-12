#ifndef LIBYB_ASYNC_DETAIL_PREPARED_TASK_HPP
#define LIBYB_ASYNC_DETAIL_PREPARED_TASK_HPP

#include "../task.hpp"
#include <memory>

namespace yb {
namespace detail {

class prepared_task;

struct prepared_task_event_sink
{
	virtual void cancel(prepared_task * promise) throw() = 0;
	virtual void cancel_and_wait(prepared_task * promise) throw() = 0;
};

class prepared_task
{
public:
	prepared_task();

	void addref() throw();
	void release() throw();

	// Shadow task interface
	void request_cancel(cancel_level cl) throw();
	void shadow_prepare_wait(task_wait_preparation_context & prep_ctx, cancel_level cl);
	void shadow_wait() throw();
	void shadow_cancel_and_wait() throw();

	// Runner interface
	void attach_event_sink(prepared_task_event_sink & r) throw();
	void detach_event_sink() throw();
	virtual void prepare_wait(task_wait_preparation_context & prep_ctx) = 0;
	virtual bool finish_wait(task_wait_finalization_context & fin_ctx) throw() = 0;
	virtual void cancel_and_wait() throw() = 0;

protected:
	~prepared_task();

	cancel_level requested_cancel_level() const throw();
	void mark_finished() throw();

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
