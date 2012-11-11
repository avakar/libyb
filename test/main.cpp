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
#include <libyb/async/mock_stream.hpp>

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
			yb::task<void> t = tmr.wait_ms(1);
			r = yb::try_run(std::move(t));
		}

		assert(!m.good() || !r.has_exception());
		assert(m.good() || r.has_exception());
	}
}

TEST_CASE(ChannelTask, "channel_task")
{
	yb::timer tmr;
	yb::channel<int> sig = yb::channel<int>::create();

	int res = 0;
	yb::task<void> t = sig.receive().then([&res](int value) -> yb::task<void> {
		res = value;
		return yb::async::value();
	});

	t |= tmr.wait_ms(1).then([&sig] { sig.send(42); });

	yb::run(std::move(t));

	assert(res == 42);
}

TEST_CASE(SignalTask, "signal_task")
{
	yb::timer tmr;
	yb::channel<void> sig = yb::channel<void>::create();

	yb::task<void> t = wait_for(sig);
	t |= tmr.wait_ms(1).then([&sig] { sig.fire(); });
	yb::run(std::move(t));
}

TEST_CASE(ReadDescriptorTask, "signal_task")
{
	static uint8_t const w1[] = { 0x80, 0x01, 0x00 };
	static uint8_t const r2[] = {
		0x80, 0x0f, 1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 0x80, 0x0f, 15, 16,
		0x00, 0x00, 0xc4, 0x91, 0x24, 0xd9, 0x46, 0x29, 0x4a, 0xef, 0xae, 0x35, 0xdd, 0x80, 0x08, 0xc3, 0x2c, 0x21, 0xb2, 0x79, 0, 0, 0,
	};

	yb::mock_stream sp;
	sp.expect_write(w1, 1);
	sp.expect_read(r2, 1);

	yb::stream_device dev;

	yb::sync_runner runner;
	yb::sync_future<void> f = runner.post(dev.run(sp));

	yb::device_descriptor dd = runner.run(yb::read_device_descriptor(dev));
	assert(dd.device_guid() == "01020304-0506-0708-090a-0b0c0d0e0f10");

	yb::device_config const * config = dd.get_config("c49124d9-4629-4aef-ae35-ddc32c21b279");
	assert(config);
}

TEST_CASE(ReadDescriptorTask_AsyncRunner, "signal_task async_runner")
{
	static uint8_t const w1[] = { 0x80, 0x01, 0x00 };
	static uint8_t const r2[] = {
		0x80, 0x0f, 1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 0x80, 0x0f, 15, 16,
		0x00, 0x00, 0xc4, 0x91, 0x24, 0xd9, 0x46, 0x29, 0x4a, 0xef, 0xae, 0x35, 0xdd, 0x80, 0x08, 0xc3, 0x2c, 0x21, 0xb2, 0x79, 0, 0, 0,
	};

	yb::mock_stream sp;
	sp.expect_write(w1, 1);
	sp.expect_read(r2, 1);

	yb::stream_device dev;

	yb::async_runner runner;
	runner.start();

	yb::async_future<void> f = runner.post(dev.run(sp));

	yb::device_descriptor dd = runner.run(yb::read_device_descriptor(dev));
	assert(dd.device_guid() == "01020304-0506-0708-090a-0b0c0d0e0f10");

	yb::device_config const * config = dd.get_config("c49124d9-4629-4aef-ae35-ddc32c21b279");
	assert(config);
}

int main(int argc, char * argv[])
{
	run_tests(argc, argv);
}
