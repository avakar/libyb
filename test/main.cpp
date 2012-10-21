#include "memmock.h"
#include "test.h"
#include <vector>

#include <libyb/async/task.hpp>
#include <libyb/async/sync_runner.hpp>
#include <libyb/async/timer.hpp>
#include <libyb/async/signal.hpp>

TEST_CASE(ValueTaskTest, "value_task")
{
	alloc_mocker m;

	while (m.next())
	{
		yb::task<int> t = yb::make_value_task(42);
	}

	assert(m.alloc_count() == 0);
}

TEST_CASE(SeqCompTask, "seqcomp_task")
{
	alloc_mocker m;
	while (m.next())
	{
		yb::task<int> t = yb::make_value_task(42).then([](int i){
			return yb::make_value_task(i+10);
		});

		assert(t.has_result());
		assert(t.get_result().get() == 52);
	}

	assert(m.alloc_count() == 0);
}

TEST_CASE(TimerTask, "timer_task")
{
	yb::timer tmr;

	alloc_mocker m;
	while (m.next())
	{
		yb::task_result<void> r;

		{
			yb::task<void> t = tmr.wait_ms(1000);
			r = yb::try_run(std::move(t));
		}

		assert(!m.good() || !r.has_exception());
		assert(m.good() || r.has_exception());
	}
}

TEST_CASE(SignalTask, "signal_task")
{
	yb::timer tmr;
	yb::signal sig;

	yb::task<void> t = wait_for(sig);
	t |= tmr.wait_ms(1).then([&sig] { sig.fire(); });
	yb::run(std::move(t));
}

int main(int argc, char * argv[])
{
	run_tests(argc, argv);
}
