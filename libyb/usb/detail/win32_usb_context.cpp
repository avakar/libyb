#include "../usb_context.hpp"
#include "usb_device_core.hpp"
#include "usb_request_context.hpp"
#include "../../async/sync_runner.hpp"
#include <map>
#include <memory>
using namespace yb;

struct usb_context::impl
{
	std::map<size_t, std::weak_ptr<detail::usb_device_core>> m_device_repository;
};

usb_context::usb_context()
	: m_pimpl(new impl())
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

		usb_device_descriptor desc;
		if (try_run(get_descriptor_ctx.get_device_descriptor(hFile.get(), desc)).has_exception())
			continue;

		std::shared_ptr<detail::usb_device_core> dev(new detail::usb_device_core());
		dev->hFile = std::move(hFile);
		dev->desc = desc;
		repo_dev = dev;
		res.push_back(usb_device(std::move(dev)));
	}

	return res;
}
