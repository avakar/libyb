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

std::vector<usb_device> usb_context::get_device_list() const
{
	std::vector<usb_device> res;
	res.reserve(m_pimpl->m_device_repository.size());

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

			uint16_t selected_langid = get_descriptor_ctx.get_default_langid(hFile.get());
			if (dev->desc.iProduct)
				dev->product = get_descriptor_ctx.get_string_descriptor_sync(hFile.get(), dev->desc.iProduct, selected_langid);
			if (dev->desc.iManufacturer)
				dev->manufacturer = get_descriptor_ctx.get_string_descriptor_sync(hFile.get(), dev->desc.iManufacturer, selected_langid);
			if (dev->desc.iSerialNumber)
				dev->serial_number = get_descriptor_ctx.get_string_descriptor_sync(hFile.get(), dev->desc.iSerialNumber, selected_langid);

			dev->hFile = std::move(hFile);
		}
		catch (...)
		{
			continue;
		}

		repo_dev = dev;
		res.push_back(usb_device(std::move(dev)));
	}

	return res;
}
