#ifndef LIBYB_ASYNC_SYNC_RUNNER_HPP
#define LIBYB_ASYNC_SYNC_RUNNER_HPP

#include "runner.hpp"
#include <memory>

namespace yb {

class sync_runner
	: public runner
{
public:
	explicit sync_runner(bool associate_thread_now = true);
	~sync_runner();

	void associate_current_thread();

protected:
	void submit(detail::prepared_task_base & pt) override;
	void run_until(detail::prepared_task_base & pt) override;

private:
	struct impl;
	std::unique_ptr<impl> m_pimpl;

	sync_runner(sync_runner const &);
	sync_runner & operator=(sync_runner const &);
};

} // namespace yb

#endif // LIBYB_ASYNC_SYNC_RUNNER_HPP
