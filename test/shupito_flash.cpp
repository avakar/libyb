#include "test.h"
#include <libyb/async/serial_port.hpp>
#include <libyb/async/stream_device.hpp>
#include <libyb/async/sync_runner.hpp>
#include <libyb/async/descriptor_reader.hpp>
#include <libyb/tunnel.hpp>
#include <libyb/usb/usb_context.hpp>
#include <libyb/usb/bulk_stream.hpp>
#include <libyb/shupito/escape_sequence.hpp>
#include <libyb/async/async_runner.hpp>
#include <libyb/async/timer.hpp>

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
#include <libyb/utils/ihex_file.hpp>
#include <fstream>

TEST_CASE(DfuFlash, "+dfu")
{
	std::ifstream fin("c:\\devel\\checkouts\\pokkus\\Debug\\pokkus.hex");
	yb::sparse_buffer program = yb::parse_ihex(fin);

	yb::usb_context usb;
	std::vector<yb::usb_device> devs = usb.get_device_list();

	std::vector<shupito_info> shupito_devs;
	for (size_t i = 0; i < devs.size(); ++i)
	{
		shupito_info shinfo;

		shinfo.desc = devs[i].descriptor();
		if (shinfo.desc.idVendor != 0x4a61 || shinfo.desc.idProduct != 0x679c)
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

//	uint8_t config = run(selected->dev.get_configuration());

	yb::async_runner runner;

	uint8_t buf[] = { 'a', 'h', 'o', 'j' };
	uint8_t read_buf[256];

	yb::async_future<size_t> readf = runner.post(selected->dev.bulk_read(0x81, read_buf, sizeof read_buf));
	runner.post_detached(selected->dev.bulk_write(2, buf, sizeof buf));
	size_t s = readf.get();

	/*r.cancel();
	r.wait();

	run(yb::wait_ms(10000));*/

	/*yb::flip2 fl;
	fl.open(selected->dev);
	
	uint8_t sig[4];
	size_t read = run(fl.read_memory(5, 0, sig, sizeof sig));

	assert(read == 4);
	assert(sig[0] == 0x1e && sig[1] == 0x95 && sig[2] == 0x41 && sig[3] == 0x04);

	run(fl.chip_erase());
	bool blank_succeeded = run(fl.blank_check(0, 0, 32*1024));
	assert(blank_succeeded);

	for (yb::sparse_buffer::region_iterator it = program.region_begin(); it != program.region_end(); ++it)
		run(fl.write_memory(0, it->first, it->second.data(), it->second.size()));

	uint8_t buffer[16*1024];
	read = run(fl.read_memory(0, 0, buffer, sizeof buffer));

	run(fl.start_application());*/
}
