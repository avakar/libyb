#include "memmock.h"
#include "test.h"
#include <vector>

#include <libyb/async/task.hpp>
#include <libyb/async/sync_runner.hpp>
#include <libyb/async/async_runner.hpp>
#include <libyb/async/timer.hpp>
#include <libyb/async/channel.hpp>
#include <libyb/async/serial_port.hpp>
#include <libyb/async/stream_device.hpp>
#include <libyb/async/descriptor_reader.hpp>

TEST_CASE(ValueTaskTest, "value_task")
{
	alloc_mocker m;

	while (m.next())
	{
		yb::task<int> t = yb::async::value(42);
	}

	assert(m.alloc_count() == 0);
}

TEST_CASE(SeqCompTask, "seqcomp_task")
{
	alloc_mocker m;
	while (m.next())
	{
		yb::task<int> t = yb::async::value(42).then([](int i){
			return yb::async::value(i+10);
		});

		assert(t.has_value());
		assert(t.get() == 52);
	}

	assert(m.alloc_count() == 0);
}

TEST_CASE(TimerTask, "timer_task")
{
	yb::sync_runner sr;
	yb::timer tmr;

	alloc_mocker m;
	while (m.next())
	{
		yb::task<void> r;

		{
			yb::task<void> t = tmr.wait_ms(1);
			r = sr.try_run(std::move(t));
		}

		assert(!m.good() || !r.has_exception());
		assert(m.good() || r.has_exception());
	}
}

TEST_CASE(ChannelTask, "channel_task")
{
	yb::timer tmr;
	yb::channel<int> sig = yb::channel<int>::create_infinite();

	int res = 0;
	yb::task<void> t = sig.receive().then([&res](int value) -> yb::task<void> {
		res = value;
		return yb::async::value();
	});

	t |= tmr.wait_ms(1).then([&sig] { sig.send(42); });

	yb::sync_runner().run(std::move(t));

	assert(res == 42);
}

TEST_CASE(SignalTask, "signal_task")
{
	yb::timer tmr;
	yb::channel<void> sig = yb::channel<void>::create_infinite();

	yb::task<void> t = wait_for(sig);
	t |= tmr.wait_ms(1).then([&sig] { sig.fire(); });
	yb::sync_runner().run(std::move(t));
}

int main(int argc, char * argv[])
{
	run_tests(argc, argv);
}
