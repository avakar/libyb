#include "test.h"
#include <libyb/async/serial_port.hpp>
#include <libyb/async/stream_device.hpp>
#include <libyb/async/sync_runner.hpp>
#include <libyb/async/descriptor_reader.hpp>
#include <libyb/tunnel.hpp>

TEST_CASE(ShupitoFlash, "+shupito")
{
	yb::sync_runner r;

	yb::serial_port sp;
	r << sp.open("COM3", 38400);

	yb::stream_device dev;
	yb::sync_future<void> f = r | dev.run(sp);

	yb::device_descriptor dd = r << yb::read_device_descriptor(dev);

	yb::tunnel_handler th;
	bool attached = th.attach(dev, dd);

	yb::tunnel_stream ts;
	r << ts.open(th, "usb");
	r << ts.fast_close();

	f.wait(yb::cl_quit);
}
