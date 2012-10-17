#include "memmock.h"
#include "test.h"
#include <vector>

#include <libyb/async/task.hpp>
#include <libyb/async/sync_runner.hpp>
#include <libyb/async/timer.hpp>

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

#if 0 //def WIN32

#include <libyb/async/task_win32.hpp>
#include <libyb/async/sync_runner.hpp>

TEST_CASE(HandleTask, "win32_handle_task")
{
	HANDLE hEvent = CreateWaitableTimer(0, TRUE, 0);

	alloc_mocker m;
	while (m.next())
	{
		yb::task<int> t = yb::make_win32_handle_task(hEvent, []{ return false; }).then([]{
			return yb::make_value_task(42);
		});

		LARGE_INTEGER li;
		li.QuadPart = -1*1000*1000*10;
		SetWaitableTimer(hEvent, &li, 0, 0, 0, FALSE);

		try
		{
			int res = yb::run(std::move(t));
			assert(res == 42);
		}
		catch (...)
		{
		}
	}

	CloseHandle(hEvent);
}

#include <libyb/async/serial_port.hpp>

TEST_CASE(SerialPortOpenTest, "serial_port")
{
	yb::serial_port sp;
	uint8_t buf[256];
	yb::task<size_t> t = sp.open("COM9").then([&sp, &buf]() {
		return sp.read(buf, sizeof buf);
	});
	t.cancel();
	yb::task_result<size_t> r = yb::try_run(std::move(t));
}

#endif

int main()
{
	run_tests();
}
