#include "../usb_context.hpp"
#include "linux_usb_device_core.hpp"
#include "../../utils/detail/scoped_unix_fd.hpp"
#include "../../async/detail/linux_fdpoll_task.hpp"
#include "../../utils/detail/pthread_mutex.hpp"
#include "udev.hpp"
#include <map>
#include <stdexcept>
#include <memory>
#include <stdio.h>
#include <libudev.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
using namespace yb;
using namespace yb::detail;

static void kill_pending_urbs(usb_device_core & core)
{
	for (std::set<urb_context *>::iterator it = core.pending_urbs.begin(); it != core.pending_urbs.end(); ++it)
	{
		urb_context * urb_ctx = *it;
		urb_ctx->urb.status = -ENODEV;
		urb_ctx->done.set_value();
	}

	core.pending_urbs.clear();
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
			if (ioctl(core->fd.get(), USBDEVFS_REAPURB, &urb) < 0)
			{
				kill_pending_urbs(*core);
				return async::raise<void>(std::runtime_error("can't reap urb"));
			}

			detail::urb_context * ctx = (detail::urb_context *)urb->usercontext;
			ctx->done.set_value();
			core->pending_urbs.erase(ctx);
			return async::value();
		}

		kill_pending_urbs(*core);
		return async::raise<void>(std::runtime_error("something bad happened to the device"));
	});
}

struct usb_context::impl
{
	async_runner & m_runner;
	pthread_mutex m_mutex;

	scoped_udev m_udev;
	scoped_udev_monitor m_udev_monitor;

	std::map<std::string, std::shared_ptr<usb_device_core> > m_devices;
	std::map<std::string, usb_device_interface> m_interfaces;

	async_future<void> m_monitor_worker;

	explicit impl(async_runner & runner)
		: m_runner(runner)
	{
	}

	~impl()
	{
		m_monitor_worker.wait(cl_quit);
	}

	std::shared_ptr<usb_device_core> handle_usb_device(
		char const * path,
		udev_device * dev)
	{
		std::map<std::string, std::shared_ptr<usb_device_core> >::const_iterator it = m_devices.find(path);
		if (it != m_devices.end())
			return it->second;

		const char * devpath = udev_device_get_devnode(dev);
		if (!devpath)
		{
			// This shouldn't happen though, all usb_device devices
			// should have a devpath.
			return std::shared_ptr<usb_device_core>();
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
			return std::shared_ptr<usb_device_core>();

		std::shared_ptr<detail::usb_device_core> core(std::make_shared<detail::usb_device_core>());
		core->fd.reset(fd.release());
		core->writable = writable;
		core->udev = m_udev;
		core->syspath = path;

		if (char const * iProduct = udev_device_get_sysattr_value(dev, "product"))
			core->iProduct = iProduct;
		if (char const * iManufacturer = udev_device_get_sysattr_value(dev, "manufacturer"))
			core->iManufacturer = iManufacturer;
		if (char const * iSerialNumber = udev_device_get_sysattr_value(dev, "serial"))
			core->iSerialNumber = iSerialNumber;

		// The read will return the descriptors in host endianity.
		if (read(core->fd.get(), &core->desc, sizeof core->desc) != sizeof core->desc)
			return std::shared_ptr<usb_device_core>();

		std::vector<uint8_t> config_desc;
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
			return std::shared_ptr<usb_device_core>();

		core->intfnames.resize(core->configs.size());
		for (size_t i = 0; i < core->configs.size(); ++i)
			core->intfnames[i].resize(core->configs[i].interfaces.size());

		// Start the dispatch loop...
		if (writable)
		{
			core->dispatch_loop = m_runner.post(loop([core](cancel_level cl) -> task<void> {
				if (cl >= cl_quit)
					return nulltask;
				return dispatch_loop(core);
			}));
		}

		m_devices.insert(std::make_pair(std::move(path), core));
		return core;
	}

	void add_device(udev_device * dev)
	{
		char const * devtype = udev_device_get_devtype(dev);
		if (!devtype)
			return;

		char const * path = udev_device_get_syspath(dev);

		if (strcmp(devtype, "usb_device") == 0)
		{
			this->handle_usb_device(path, dev);
		}
		else if (strcmp(devtype, "usb_interface") == 0)
		{
			std::map<std::string, usb_device_interface>::const_iterator it = m_interfaces.find(path);
			if (it != m_interfaces.end())
				return;

			char const * sysname = udev_device_get_sysname(dev);
			char const * config_intf_id = strchr(sysname, ':');
			if (!config_intf_id)
				return;

			size_t config_value;
			size_t intf_index;
			if (sscanf(config_intf_id, ":%ju.%ju", &config_value, &intf_index) != 2)
				return;

			struct udev_device * par = udev_device_get_parent(dev);
			char const * sys_parpath = udev_device_get_syspath(par);

			std::shared_ptr<usb_device_core> const & core = handle_usb_device(sys_parpath, par);
			if (!core)
				return;

			for (size_t i = 0; i < core->configs.size(); ++i)
			{
				if (core->configs[i].bConfigurationValue == config_value)
				{
					if (intf_index >= core->intfnames[i].size())
						break;

					if (char const * intfname = udev_device_get_sysattr_value(dev, "interface"))
						core->intfnames[i][intf_index] = intfname;
					else
						core->intfnames[i][intf_index].clear();

					usb_device_interface intf(core, i, intf_index);
					m_interfaces.insert(std::make_pair(path, intf));
					break;
				}
			}
		}
	}

	void remove_device(udev_device * dev)
	{
		std::string path = udev_device_get_syspath(dev);
		m_devices.erase(path);
		m_interfaces.erase(path);
	}

	void handle_monitor(short)
	{
		scoped_udev_device dev(udev_monitor_receive_device(m_udev_monitor.get()));
		if (dev.empty())
			return;

		char const * action = udev_device_get_action(dev.get());
		if (strcmp(action, "add") == 0)
		{
			scoped_pthread_lock l(m_mutex);
			this->add_device(dev.get());
		}
		else if (strcmp(action, "remove") == 0)
		{
			scoped_pthread_lock l(m_mutex);
			this->remove_device(dev.get());
		}
	}

	void enumerate_all()
	{
		scoped_udev_enumerate e(udev_enumerate_new(m_udev.get()));

		udev_enumerate_add_match_subsystem(e, "usb");
		udev_check_error(udev_enumerate_scan_devices(e));

		struct udev_list_entry * head = udev_enumerate_get_list_entry(e);
		struct udev_list_entry * p;

		scoped_pthread_lock l(m_mutex);
		udev_list_entry_foreach(p, head)
		{
			char const * path = udev_list_entry_get_name(p);

			scoped_udev_device dev(udev_device_new_from_syspath(m_udev.get(), path));
			if (dev.empty())
				throw std::runtime_error("failed to create udev device");

			this->add_device(dev.get());
		}
	}
};

usb_context::usb_context(async_runner & runner)
	: m_pimpl(new impl(runner))
{
	m_pimpl->m_udev.reset(udev_new());
	if (m_pimpl->m_udev.empty())
		throw std::runtime_error("failed to create udev context");

	m_pimpl->m_udev_monitor.reset(udev_monitor_new_from_netlink(m_pimpl->m_udev.get(), "udev"));
	if (m_pimpl->m_udev_monitor.empty())
		throw std::runtime_error("failed to create udev monitor");
	udev_check_error(udev_monitor_filter_add_match_subsystem_devtype(m_pimpl->m_udev_monitor.get(), "usb", "usb_device"));
	udev_check_error(udev_monitor_filter_add_match_subsystem_devtype(m_pimpl->m_udev_monitor.get(), "usb", "usb_interface"));
	udev_check_error(udev_monitor_enable_receiving(m_pimpl->m_udev_monitor.get()));

	m_pimpl->enumerate_all();

	int fd = udev_monitor_get_fd(m_pimpl->m_udev_monitor.get());
	m_pimpl->m_monitor_worker = runner.post(yb::loop(async::value((short)0), [this, fd](short revents, cancel_level cl) -> task<short> {
		if (revents & POLLIN)
			m_pimpl->handle_monitor(revents);
		return cl >= cl_quit? nulltask: yb::make_linux_pollfd_task(fd, POLLIN, [](cancel_level){
			return false;
		});
	}));
}

usb_context::~usb_context()
{
}

task<void> usb_context::run()
{
	return async::value();
}

void usb_context::get_device_list(std::vector<usb_device> & devs, std::vector<usb_device_interface> & intfs) const
{
	std::vector<usb_device> new_devs;
	std::vector<usb_device_interface> new_intfs;

	{
		scoped_pthread_lock l(m_pimpl->m_mutex);
		for (std::map<std::string, std::shared_ptr<usb_device_core> >::const_iterator it = m_pimpl->m_devices.begin(); it != m_pimpl->m_devices.end(); ++it)
			new_devs.push_back(usb_device(it->second));

		for (std::map<std::string, usb_device_interface>::const_iterator it = m_pimpl->m_interfaces.begin(); it != m_pimpl->m_interfaces.end(); ++it)
			new_intfs.push_back(it->second);
	}

	devs.swap(new_devs);
	intfs.swap(new_intfs);
}
