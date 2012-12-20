#include "../usb_context.hpp"
#include "win32_usb_device_core.hpp"
#include "usb_request_context.hpp"
#include "../../async/sync_runner.hpp"
#include <map>
#include <memory>
using namespace yb;

struct usb_context::impl
	: noncopyable
{
	async_runner & m_runner;
	std::map<size_t, std::weak_ptr<detail::usb_device_core>> m_device_repository;

	impl(async_runner & runner)
		: m_runner(runner)
	{
	}
};

usb_context::usb_context(async_runner & runner)
	: m_pimpl(new impl(runner))
{
}

usb_context::~usb_context()
{
}

void usb_context::get_device_list(std::vector<usb_device> & devs, std::vector<usb_device_interface> & intfs) const
{
	std::vector<usb_device> res;
	std::vector<usb_device_interface> intf_res;
	res.reserve(m_pimpl->m_device_repository.size());
	intf_res.reserve(res.capacity());

	detail::usb_request_context get_descriptor_ctx;
	for (size_t i = 1; i < LIBUSB_MAX_NUMBER_OF_DEVICES; ++i)
	{
		WCHAR devname[32];
		wsprintf(devname, L"\\\\.\\libusb0-%04d", i);

		detail::scoped_win32_handle hFile(CreateFileW(devname, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL));
		if (hFile.empty())
			continue;

		std::weak_ptr<detail::usb_device_core> & repo_dev = m_pimpl->m_device_repository[i];
		if (std::shared_ptr<detail::usb_device_core> dev = repo_dev.lock())
		{
			res.push_back(usb_device(std::move(dev)));
			continue;
		}

		std::shared_ptr<detail::usb_device_core> dev(new detail::usb_device_core());
		try
		{
			get_descriptor_ctx.get_device_descriptor(hFile.get(), dev->desc);

			for (uint8_t i = 0; i < dev->desc.bNumConfigurations; ++i)
			{
				uint8_t config_desc_header[4];
				get_descriptor_ctx.get_descriptor(hFile.get(), 2, i, 0, config_desc_header, sizeof config_desc_header);

				uint16_t wTotalLength = config_desc_header[2] | (config_desc_header[3] << 8);

				std::vector<uint8_t> config_desc(wTotalLength);
				get_descriptor_ctx.get_descriptor(hFile.get(), 2, i, 0, config_desc.data(), config_desc.size());

				dev->configs.push_back(parse_config_descriptor(config_desc));
			}

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
		res.push_back(usb_device(std::move(dev)));
	}

	for (size_t i = 0; i < res.size(); ++i)
	{
		usb_device & dev = res[i];
		std::shared_ptr<detail::usb_device_core> core = dev.core();

		uint8_t active_config = dev.get_cached_configuration();

		usb_config_descriptor * config = 0;
		if (core->active_config != active_config)
		{
			core->active_config = active_config;
			for (size_t i = 0; i < core->configs.size(); ++i)
			{
				if (core->configs[i].bConfigurationValue == core->active_config)
				{
					config = &core->configs[i];
					core->active_config_index = i;
					break;
				}
			}

			if (!config)
				core->active_config_index = core->configs.size();
		}
		else if (core->active_config_index < core->configs.size())
		{
			config = &core->configs[core->active_config_index];
		}

		if (config)
		{
			for (size_t i = 0; i < config->interfaces.size(); ++i)
				intf_res.push_back(usb_device_interface(core, core->active_config_index, i));
		}
	}

	devs.swap(res);
	intfs.swap(intf_res);
}
