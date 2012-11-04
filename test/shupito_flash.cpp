#include "test.h"
#include <libyb/async/serial_port.hpp>
#include <libyb/async/stream_device.hpp>
#include <libyb/async/sync_runner.hpp>
#include <libyb/async/descriptor_reader.hpp>
#include <libyb/tunnel.hpp>
#include <libyb/usb/usb.h>
#include <libyb/usb/bulk_stream.hpp>

TEST_CASE(ShupitoFlash, "+shupito")
{
	yb::usb_context usb;
	std::vector<yb::usb_device> devs = usb.get_device_list();

	yb::usb_device * selected = 0;
	for (size_t i = 0; i < devs.size(); ++i)
	{
		yb::usb_device_descriptor desc = devs[i].descriptor();
		if (desc.idVendor == 0x4a61 && desc.idProduct == 0x679a)
		{
			selected = &devs[i];
			break;
		}
	}

	yb::usb_bulk_stream bs;
	yb::usb_interface_guard ig;
	yb::serial_port sp;

	yb::sync_runner r;

	if (selected)
	{
		yb::usb_config_descriptor cd = selected->get_config_descriptor();
		r << bs.claim_and_open(*selected, ig, cd);
	}

	if (bs.is_readable())
		r |= discard(bs);

	r << sp.open("COM3", 38400);

	yb::stream_device dev;
	yb::sync_future<void> f = r | dev.run(sp);

	yb::device_descriptor dd = r << yb::read_device_descriptor(dev);

	yb::tunnel_handler th;
	bool attached = th.attach(dev, dd);

	yb::tunnel_stream ts;
	r << ts.open(th, "usb");

	static uint8_t const escape_seq[] = { 't', '~', 'z', '3' };
	r << ts.write_all(escape_seq, sizeof escape_seq);

	r << ts.fast_close();

	f.wait(yb::cl_quit);
}
