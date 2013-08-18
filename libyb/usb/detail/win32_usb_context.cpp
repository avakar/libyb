#include "../usb_context.hpp"
#include "win32_usb_device_core.hpp"
#include "usb_request_context.hpp"
#include "../../async/async_runner.hpp"
#include "../../async/timer.hpp"
#include <map>
#include <memory>
using namespace yb;

struct usb_context::impl
	: noncopyable
{
	runner & m_runner;

	std::vector<std::shared_ptr<detail::usb_device_core>> m_devices;
	std::map<size_t, std::weak_ptr<detail::usb_device_core>> m_device_repository;

	timer m_refresh_timer;
	std::function<void (usb_plugin_event const &)> m_event_sink;

	impl(runner & r)
		: m_runner(r), m_devices(LIBUSB_MAX_NUMBER_OF_DEVICES)
	{
	}

	task<void> run_one()
	{
		return m_refresh_timer.wait_ms(1000).then([this] {
			this->refresh_device_list();
		});
	}

	void refresh_device_list();
};

usb_context::usb_context(runner & r)
	: m_pimpl(new impl(r))
{
}

usb_context::~usb_context()
{
}

task<void> usb_context::run(std::function<void (usb_plugin_event const &)> const & event_sink)
{
	m_pimpl->m_event_sink = event_sink;
	m_pimpl->refresh_device_list();

	return m_pimpl->m_runner.post(yb::loop([this](cancel_level cl) -> task<void> {
		return cl >= cl_quit? nulltask: m_pimpl->run_one();
	}));
}

void usb_context::impl::refresh_device_list()
{
	std::vector<usb_device> res;
	std::vector<usb_device_interface> intf_res;
	res.reserve(m_device_repository.size());
	intf_res.reserve(res.capacity());

	detail::usb_request_context get_descriptor_ctx;
	for (size_t i = 1; i < LIBUSB_MAX_NUMBER_OF_DEVICES; ++i)
	{
		WCHAR devname[32];
		wsprintf(devname, L"\\\\.\\libusb0-%04d", i);

		detail::scoped_win32_handle hFile(CreateFileW(devname, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL));
		if (hFile.empty())
		{
			if (std::shared_ptr<detail::usb_device_core> core = m_devices[i])
			{
				m_devices[i].reset();

				if (core->active_config_index < core->configs.size())
				{
					usb_config_descriptor * config = &core->configs[core->active_config_index];
					for (size_t i = 0; i < config->interfaces.size(); ++i)
					{
						usb_plugin_event pe;
						pe.action = usb_plugin_event::a_remove;
						pe.intf = usb_device_interface(core, core->active_config_index, i);
						m_event_sink(pe);
					}
				}

				usb_plugin_event dp;
				dp.action = usb_plugin_event::a_remove;
				dp.dev = usb_device(core);
				m_event_sink(dp);
			}
			continue;
		}

		std::weak_ptr<detail::usb_device_core> & repo_dev = m_device_repository[i];
		if (std::shared_ptr<detail::usb_device_core> dev = repo_dev.lock())
			continue;

		std::shared_ptr<detail::usb_device_core> dev(new detail::usb_device_core());
		try
		{
			get_descriptor_ctx.get_device_descriptor(hFile.get(), dev->desc);

			bool invalid_config_desc = false;
			for (uint8_t i = 0; i < dev->desc.bNumConfigurations; ++i)
			{
				uint8_t config_desc_header[4];
				size_t r = get_descriptor_ctx.get_descriptor_sync(hFile.get(), 2, i, 0, config_desc_header, sizeof config_desc_header);
				if (r < 4)
				{
					invalid_config_desc = true;
					break;
				}

				uint16_t wTotalLength = config_desc_header[2] | (config_desc_header[3] << 8);

				std::vector<uint8_t> config_desc(wTotalLength);
				r = get_descriptor_ctx.get_descriptor_sync(hFile.get(), 2, i, 0, config_desc.data(), config_desc.size());
				if (r != wTotalLength)
				{
					invalid_config_desc = true;
					break;
				}

				dev->configs.push_back(parse_config_descriptor(config_desc));
			}

			if (invalid_config_desc)
				continue;

			dev->active_config = -1;
			dev->active_config_index = dev->configs.size();

			dev->selected_langid = get_descriptor_ctx.get_default_langid(hFile.get());

			dev->interface_names.resize(dev->configs.size());
			for (size_t i = 0; i < dev->configs.size(); ++i)
			{
				dev->interface_names[i].resize(dev->configs[i].interfaces.size());
				for (size_t j = 0; j < dev->configs[i].interfaces.size(); ++j)
				{
					if (dev->configs[i].interfaces[j].altsettings.empty())
						continue;
					if (uint8_t iInterface = dev->configs[i].interfaces[j].altsettings[0].iInterface)
						dev->interface_names[i][j] = get_descriptor_ctx.get_string_descriptor_sync(hFile.get(), iInterface, dev->selected_langid);
				}
			}

			if (dev->desc.iProduct)
				dev->product = get_descriptor_ctx.get_string_descriptor_sync(hFile.get(), dev->desc.iProduct, dev->selected_langid);
			if (dev->desc.iManufacturer)
				dev->manufacturer = get_descriptor_ctx.get_string_descriptor_sync(hFile.get(), dev->desc.iManufacturer, dev->selected_langid);
			if (dev->desc.iSerialNumber)
				dev->serial_number = get_descriptor_ctx.get_string_descriptor_sync(hFile.get(), dev->desc.iSerialNumber, dev->selected_langid);

			dev->hFile = std::move(hFile);
		}
		catch (...)
		{
			continue;
		}

		repo_dev = dev;
		m_devices[i] = dev;

		usb_plugin_event dp;
		dp.action = usb_plugin_event::a_add;
		dp.dev = usb_device(dev);
		m_event_sink(dp);
	}

	for (size_t i = 0; i < m_devices.size(); ++i)
	{
		if (!m_devices[i])
			continue;

		std::shared_ptr<detail::usb_device_core> core = m_devices[i];
		usb_device dev = usb_device(core);

		uint8_t active_config = dev.get_cached_configuration();
		if (core->active_config == active_config)
			continue;

		if (core->active_config_index < core->configs.size())
		{
			usb_config_descriptor * config = &core->configs[core->active_config_index];
			for (size_t i = 0; i < config->interfaces.size(); ++i)
			{
				usb_plugin_event pe;
				pe.action = usb_plugin_event::a_remove;
				pe.intf = usb_device_interface(core, core->active_config_index, i);
				m_event_sink(pe);
			}
		}

		core->active_config = active_config;
		core->active_config_index = core->configs.size();
		for (size_t i = 0; i < core->configs.size(); ++i)
		{
			if (core->configs[i].bConfigurationValue == core->active_config)
			{
				core->active_config_index = i;
				break;
			}
		}

		if (core->active_config_index < core->configs.size())
		{
			usb_config_descriptor * config = &core->configs[core->active_config_index];
			for (size_t i = 0; i < config->interfaces.size(); ++i)
			{
				usb_plugin_event pe;
				pe.action = usb_plugin_event::a_add;
				pe.intf = usb_device_interface(core, core->active_config_index, i);
				m_event_sink(pe);
			}
		}
	}
}
