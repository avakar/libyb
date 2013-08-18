#ifndef LIBYB_ASYNC_ASYNC_RUNNER_HPP
#define LIBYB_ASYNC_ASYNC_RUNNER_HPP

#include "runner.hpp"
#include <memory>

namespace yb {

class async_runner
	: public runner
{
public:
	async_runner();
	~async_runner();

	void submit(detail::prepared_task * pt) override;
	void run_until(detail::prepared_task * pt) override;

private:
	struct impl;
	std::unique_ptr<impl> m_pimpl;
};

} // namespace yb

#endif // LIBYB_ASYNC_ASYNC_RUNNER_HPP
