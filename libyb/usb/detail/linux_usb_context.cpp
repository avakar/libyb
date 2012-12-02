#include "../usb_context.hpp"
#include "linux_usb_device_core.hpp"
#include "../../utils/detail/scoped_unix_fd.hpp"
#include "../../async/detail/linux_fdpoll_task.hpp"
#include "udev.hpp"
#include <map>
#include <stdexcept>
#include <memory>
#include <libudev.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
using namespace yb;
using namespace yb::detail;

struct usb_context::impl
{
	scoped_udev m_udev;
	std::map<std::string, std::shared_ptr<usb_device_core> > m_devices;
	async_runner & m_runner;

	explicit impl(async_runner & runner)
		: m_runner(runner)
	{
	}
};

usb_context::usb_context(async_runner & runner)
	: m_pimpl(new impl(runner))
{
	m_pimpl->m_udev.reset(udev_new());
	if (m_pimpl->m_udev.empty())
		throw std::runtime_error("failed to create udev context");
}

usb_context::~usb_context()
{
}

task<void> usb_context::run()
{
	return async::value();
}

static task<void> dispatch_loop(std::shared_ptr<usb_device_core> const & core)
{
	// FIXME: The loop must be nothrow as it is in the cancel path
	// for other requests. Find a way to do this somehow.
	return make_linux_pollfd_task(core->fd.get(), POLLOUT, [](cancel_level cl) {
		return cl < cl_quit;
	}).then([core](short revents) -> task<void> {
		if (revents & POLLOUT)
		{
			struct usbdevfs_urb * urb;
			if (ioctl(core->fd.get(), USBDEVFS_REAPURBNDELAY, &urb) < 0)
				return async::raise<void>(std::runtime_error("can't reap urb"));
			((detail::urb_context *)urb->usercontext)->done.set_value();
			return async::value();
		}

		return async::raise<void>(std::runtime_error("something bad happened to the device"));
	});
}

std::vector<usb_device> usb_context::get_device_list() const
{
	std::vector<usb_device> res;
	std::map<std::string, std::shared_ptr<usb_device_core> > new_devices;

	scoped_udev_enumerate e(udev_enumerate_new(m_pimpl->m_udev.get()));

	//udev_check_error(udev_enumerate_add_match_property(e, "DRIVER", "usb"));
	udev_enumerate_add_match_subsystem(e, "usb");
	udev_check_error(udev_enumerate_scan_devices(e));

	struct udev_list_entry * head = udev_enumerate_get_list_entry(e);
	struct udev_list_entry * p;

	std::vector<uint8_t> config_desc;
	udev_list_entry_foreach(p, head)
	{
		std::string path = udev_list_entry_get_name(p);

		std::map<std::string, std::shared_ptr<usb_device_core> >::const_iterator it = m_pimpl->m_devices.find(path);
		if (it != m_pimpl->m_devices.end())
		{
			usb_device(it->second).get_cached_configuration();
			new_devices.insert(std::make_pair(std::move(path), it->second));
			res.push_back(usb_device(it->second));
			continue;
		}

		scoped_udev_device dev(udev_device_new_from_syspath(m_pimpl->m_udev.get(), path.c_str()));
		if (dev.empty())
			throw std::runtime_error("failed to create udev device");

		char const * devtype = udev_device_get_devtype(dev.get());
		if (strcmp(devtype, "usb_device") != 0)
			continue;

		const char * devpath = udev_device_get_devnode(dev.get());
		if (!devpath)
		{
			// This shouldn't happen though, all usb_device devices
			// should have a devpath.
			continue;
		}

		// TODO: do we really care about devices that we can't control?
		bool writable = true;

		detail::scoped_unix_fd fd(open(devpath, O_RDWR | O_NOCTTY | O_NONBLOCK));
		if (fd.empty() && errno == EACCES)
		{
			fd.reset(open(devpath, O_RDONLY | O_NOCTTY | O_NONBLOCK));
			writable = false;
		}

		if (fd.empty())
			continue;

		std::shared_ptr<detail::usb_device_core> core(std::make_shared<detail::usb_device_core>());
		core->fd.reset(fd.release());
		core->writable = writable;
		core->udev = m_pimpl->m_udev;
		core->syspath = path;

		if (char const * iProduct = udev_device_get_sysattr_value(dev.get(), "product"))
			core->iProduct = iProduct;
		if (char const * iManufacturer = udev_device_get_sysattr_value(dev.get(), "manufacturer"))
			core->iManufacturer = iManufacturer;
		if (char const * iSerialNumber = udev_device_get_sysattr_value(dev.get(), "serial"))
			core->iSerialNumber = iSerialNumber;

		// The read will return the descriptors in host endianity.
		if (read(core->fd.get(), &core->desc, sizeof core->desc) != sizeof core->desc)
			continue;

		/*if (core->desc.idVendor != 0x4a61)
			continue;*/

		bool configs_ok = true;
		for (size_t i = 0; i < core->desc.bNumConfigurations; ++i)
		{
			config_desc.resize(4);
			if (read(core->fd.get(), config_desc.data(), 4) != 4)
			{
				configs_ok = false;
				break;
			}

			uint16_t wTotalLength = config_desc[2] | (config_desc[3] << 8);
			if (wTotalLength < sizeof(usb_raw_config_descriptor))
			{
				configs_ok = false;
				break;
			}

			config_desc.resize(wTotalLength);
			if (read(core->fd.get(), config_desc.data() + 4, wTotalLength - 4) != wTotalLength - 4)
			{
				configs_ok = false;
				break;
			}

			core->configs.push_back(parse_config_descriptor(config_desc));
		}

		if (!configs_ok)
			continue;

		// Start the dispatch loop...
		if (writable)
		{
			core->dispatch_loop = m_pimpl->m_runner.post(loop([core](cancel_level cl) -> task<void> {
				if (cl >= cl_quit)
					return nulltask;
				return dispatch_loop(core);
			}));
		}

		new_devices.insert(std::make_pair(std::move(path), core));
		res.push_back(usb_device(core));
	}

	m_pimpl->m_devices.swap(new_devices);
	return res;
}
