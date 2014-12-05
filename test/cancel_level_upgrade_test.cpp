#include <libyb/async/task.hpp>
#include "test.h"

namespace {

struct stats
{
	yb::cancel_level m_cl;
	size_t m_cancel_call_count;
};

struct mock_task
	: public yb::task_base<int>
{
	explicit mock_task(stats & s)
		: m_stats(s)
	{
	}

	yb::task<int> start(yb::runner_registry & rr, yb::task_completion_sink<int> & sink) override
	{
		return yb::nulltask;
	}

	yb::task<int> cancel(yb::runner_registry * rr, yb::cancel_level cl) throw() override
	{
		m_stats.m_cl = cl;
		++m_stats.m_cancel_call_count;
		return yb::nulltask;
	}

	stats & m_stats;
};

}

TEST_CASE(CancelLevelUpgradeTest, "")
{
	stats s = {};
	yb::task<int> t = yb::task<int>::from_task(new mock_task(s));

	assert(s.m_cl == yb::cl_none);
	assert(s.m_cancel_call_count == 0);

	t.cancel(yb::cl_none);
	assert(s.m_cl == yb::cl_none);
	assert(s.m_cancel_call_count == 0);

	t.cancel(yb::cl_quit);
	assert(s.m_cl == yb::cl_quit);
	assert(s.m_cancel_call_count == 1);

	t.cancel(yb::cl_quit);
	assert(s.m_cl == yb::cl_quit);
	assert(s.m_cancel_call_count == 1);

	t.cancel(yb::cl_none);
	assert(s.m_cl == yb::cl_quit);
	assert(s.m_cancel_call_count == 1);

	t.cancel(yb::cl_abort);
	assert(s.m_cl == yb::cl_abort);
	assert(s.m_cancel_call_count == 2);

	t.cancel(yb::cl_kill);
	assert(s.m_cl == yb::cl_kill);
	assert(s.m_cancel_call_count == 3);

	t.cancel(yb::cl_quit);
	assert(s.m_cl == yb::cl_kill);
	assert(s.m_cancel_call_count == 3);
}

