#include "../resumable.hpp"
#include "win32_handle_task.hpp"
#include <windows.h>

struct yb::resumer::impl
{
	void * master_fiber;
	void * self_fiber;
	yb::task<void> current_task;
	yb::task<void> completion_task;
	std::function<void(yb::resumer &)> fn;
};

void yb::resumer::run(yb::task<void> && t)
{
	m_pimpl->current_task = std::move(t);
	SwitchToFiber(m_pimpl->master_fiber);
}

static void CALLBACK resumable_fiber_proc(void * param)
{
	yb::resumer::impl * rc = (yb::resumer::impl *)param;
	try
	{
		yb::resumer r(rc);
		rc->fn(r);
		rc->completion_task = yb::async::value();
	}
	catch (...)
	{
		rc->completion_task = yb::async::raise<void>();
	}

	SwitchToFiber(rc->master_fiber);
}

yb::task<void> yb::make_resumable(std::function<void(resumer &)> const & f)
{
	return yb::async::fix_affinity().then([f]() -> yb::task<void> {
		void * master_fiber;
		if (!IsThreadAFiber())
		{
			master_fiber = ConvertThreadToFiber(0);
			if (master_fiber == nullptr)
				return yb::async::raise<void>(std::runtime_error("failed to convert thread to fiber"));
		}
		else
		{
			master_fiber = GetCurrentFiber();
		}

		std::shared_ptr<resumer::impl> rc = std::make_shared<resumer::impl>();
		rc->self_fiber = CreateFiber(0, &resumable_fiber_proc, rc.get());
		if (rc->self_fiber == nullptr)
			return yb::async::raise<void>(std::runtime_error("failed to create fiber"));

		rc->master_fiber = master_fiber;
		rc->fn = f;

		return yb::loop([rc](cancel_level) -> yb::task<void> {
			SwitchToFiber(rc->self_fiber);
			if (rc->completion_task.has_result())
				return yb::nulltask;
			return std::move(rc->current_task);
		}).continue_with([rc](task_result<void> r) {
			DeleteFiber(rc->self_fiber);
			return std::move(rc->completion_task);
		});
	});
}
