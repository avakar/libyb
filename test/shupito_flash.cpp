#include "test.h"
#include <libyb/async/serial_port.hpp>
#include <libyb/async/stream_device.hpp>
#include <libyb/async/sync_runner.hpp>
#include <libyb/async/descriptor_reader.hpp>
#include <libyb/tunnel.hpp>
#include <libyb/usb/usb.h>
#include <libyb/usb/bulk_stream.hpp>
#include <libyb/shupito/escape_sequence.hpp>

struct shupito_info
{
	yb::usb_device dev;
	yb::usb_device_descriptor desc;
	yb::usb_config_descriptor config;
	uint16_t selected_langid;
	std::string manufacturer, product, sn;
};

TEST_CASE(ShupitoFlash, "+shupito")
{
	yb::usb_context usb;
	std::vector<yb::usb_device> devs = usb.get_device_list();

	std::vector<shupito_info> shupito_devs;
	for (size_t i = 0; i < devs.size(); ++i)
	{
		shupito_info shinfo;
		shinfo.desc = devs[i].descriptor();
		if (shinfo.desc.idVendor != 0x4a61 || shinfo.desc.idProduct != 0x679a)
			continue;

		shinfo.dev = devs[i];

		std::vector<uint16_t> langids;
		langids = shinfo.dev.get_langid_list();

		if (langids.empty() || std::find(langids.begin(), langids.end(), 0x409) != langids.end())
			shinfo.selected_langid = 0x409;
		else
			shinfo.selected_langid = langids[0];

		if (shinfo.desc.iManufacturer)
			shinfo.manufacturer = shinfo.dev.get_string_descriptor(shinfo.desc.iManufacturer, shinfo.selected_langid);
		if (shinfo.desc.iProduct)
			shinfo.product = shinfo.dev.get_string_descriptor(shinfo.desc.iProduct, shinfo.selected_langid);
		if (shinfo.desc.iSerialNumber)
			shinfo.sn = shinfo.dev.get_string_descriptor(shinfo.desc.iSerialNumber, shinfo.selected_langid);

		shupito_devs.push_back(shinfo);
	}

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
	r << send_avr232boot_escape_seq(ts, 1000);

	static uint8_t const unescape_seq[] = { 0x11 };
	r << ts.write_all(unescape_seq, sizeof unescape_seq);

	r << ts.fast_close();

	f.wait(yb::cl_quit);
}

#include <libyb/shupito/flip2.hpp>

TEST_CASE(DfuFlash, "+dfu")
{
	yb::usb_context usb;
	std::vector<yb::usb_device> devs = usb.get_device_list();

	std::vector<shupito_info> shupito_devs;
	for (size_t i = 0; i < devs.size(); ++i)
	{
		shupito_info shinfo;

		shinfo.desc = devs[i].descriptor();
		if (shinfo.desc.idVendor != 0x03eb || shinfo.desc.idProduct != 0x2fe4)
			continue;

		shinfo.config = devs[i].get_config_descriptor();

		shinfo.dev = devs[i];

		std::vector<uint16_t> langids;
		langids = shinfo.dev.get_langid_list();

		if (langids.empty() || std::find(langids.begin(), langids.end(), 0x409) != langids.end())
			shinfo.selected_langid = 0x409;
		else
			shinfo.selected_langid = langids[0];

		if (shinfo.desc.iManufacturer)
			shinfo.manufacturer = shinfo.dev.get_string_descriptor(shinfo.desc.iManufacturer, shinfo.selected_langid);
		if (shinfo.desc.iProduct)
			shinfo.product = shinfo.dev.get_string_descriptor(shinfo.desc.iProduct, shinfo.selected_langid);
		if (shinfo.desc.iSerialNumber)
			shinfo.sn = shinfo.dev.get_string_descriptor(shinfo.desc.iSerialNumber, shinfo.selected_langid);

		shupito_devs.push_back(shinfo);
	}

	shupito_info * selected = 0;
	if (!shupito_devs.empty())
		selected = &shupito_devs[0];

	if (!selected)
		return;

	yb::flip2 fl;
	fl.open(selected->dev);

	uint8_t sig[4];
	size_t read = run(fl.read_memory(5, 0, sig, sizeof sig));

	assert(read == 4);

	uint8_t buffer[16*1024];
	read = run(fl.read_memory(0, 0, buffer, sizeof buffer));

	bool blank_succeeded = run(fl.blank_check(0, 0, 32*1024));
}

