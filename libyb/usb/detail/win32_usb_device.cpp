#include "../usb_device.hpp"
#include "win32_usb_device_core.hpp"
#include "usb_request_context.hpp"
#include "../../async/sync_runner.hpp"
#include "../../async/detail/win32_handle_task.hpp"
#include "../../utils/utf.hpp"
using namespace yb;

usb_device_descriptor usb_device::descriptor() const
{
	return m_core->desc;
}

usb_config_descriptor usb_device::get_config_descriptor() const
{
	detail::usb_request_context ctx;

	uint8_t desc_header[4];
	if (ctx.get_descriptor_sync(m_core->hFile.get(), 2, 0, 0, desc_header, sizeof desc_header) != sizeof desc_header)
		throw std::runtime_error("request failure");

	uint16_t wTotalLength = desc_header[2] | (desc_header[3] << 8);
	if (wTotalLength < usb_raw_config_descriptor::size)
		throw std::runtime_error("invalid config descriptor");

	std::vector<uint8_t> desc(wTotalLength);

	for (;;)
	{
		size_t len = ctx.get_descriptor_sync(m_core->hFile.get(), 2, 0, 0, desc.data(), desc.size());
		if (len < usb_raw_config_descriptor::size)
			throw std::runtime_error("invalid config descriptor");

		wTotalLength = desc[2] | (desc[3] << 8);
		if (wTotalLength < usb_raw_config_descriptor::size)
			throw std::runtime_error("invalid config descriptor");

		if (desc.size() >= wTotalLength)
		{
			if (len < wTotalLength)
				throw std::runtime_error("invalid config descriptor");

			desc.resize(wTotalLength);
			break;
		}

		desc.resize(wTotalLength);
	}

	return parse_config_descriptor(desc);
}

void usb_device::set_interface(uint8_t intfno, uint8_t altsetting)
{
	detail::usb_request_context ctx;
	ctx.set_interface(m_core->hFile.get(), intfno, altsetting);
}

uint32_t usb_device::vidpid() const
{
	return ((uint32_t)m_core->desc.idVendor << 16) | m_core->desc.idProduct;
}

uint16_t usb_device::get_default_langid() const
{
	detail::usb_request_context ctx;
	return ctx.get_default_langid(m_core->hFile.get());
}

std::vector<uint16_t> usb_device::get_langid_list()
{
	detail::usb_request_context ctx;

	uint8_t buf[256];
	size_t read = ctx.get_descriptor_sync(m_core->hFile.get(), 3, 0, 0, buf, sizeof buf);
	if (read < 2 || buf[0] != read || buf[1] != 3 || read % 2 != 0)
		throw std::runtime_error("invalid string descriptor");

	std::vector<uint16_t> res;
	for (size_t i = 2; i < read; i += 2)
		res.push_back(buf[i] | (buf[i+1] << 8));
	return res;
}

std::string usb_device::get_string_descriptor(uint8_t index, uint16_t langid)
{
	detail::usb_request_context ctx;
	return ctx.get_string_descriptor_sync(m_core->hFile.get(), index, langid);
}

std::string usb_device::product() const
{
	return m_core->product;
}

std::string usb_device::manufacturer() const
{
	return m_core->manufacturer;
}

std::string usb_device::serial_number() const
{
	return m_core->serial_number;
}

uint8_t usb_device::get_cached_configuration() const
{
	detail::usb_request_context ctx;
	return ctx.get_configuration(m_core->hFile.get());
}

task<uint8_t> usb_device::get_configuration()
{
	try
	{
		detail::usb_request_context ctx;
		return async::value(ctx.get_configuration(m_core->hFile.get()));
	}
	catch (...)
	{
		return async::raise<uint8_t>();
	}
}

task<void> usb_device::set_configuration(uint8_t config)
{
	try
	{
		std::shared_ptr<detail::usb_request_context> ctx(new detail::usb_request_context());
		return ctx->set_configuration(m_core->hFile.get(), config).follow_with([ctx](){});
	}
	catch (...)
	{
		return async::raise<void>();
	}
}

bool usb_device::claim_interface(uint8_t intfno) const
{
	detail::usb_request_context ctx;
	return ctx.claim_interface(m_core->hFile.get(), intfno);
}

void usb_device::release_interface(uint8_t intfno) const
{
	detail::usb_request_context ctx;
	ctx.release_interface(m_core->hFile.get(), intfno);
}

task<size_t> usb_device::bulk_read(usb_endpoint_t ep, uint8_t * buffer, size_t size) const
{
	try
	{
		std::shared_ptr<detail::usb_request_context> ctx(new detail::usb_request_context());
		return ctx->bulk_read(m_core->hFile.get(), ep, buffer, size).follow_with([ctx](size_t){});
	}
	catch (...)
	{
		return async::raise<size_t>();
	}
}

task<size_t> usb_device::bulk_write(usb_endpoint_t ep, uint8_t const * buffer, size_t size) const
{
	try
	{
		std::shared_ptr<detail::usb_request_context> ctx(new detail::usb_request_context());
		return ctx->bulk_write(m_core->hFile.get(), ep, buffer, size).follow_with([ctx](size_t){});
	}
	catch (...)
	{
		return async::raise<size_t>();
	}
}

task<size_t> usb_device::control_read(uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint8_t * buffer, size_t size)
{
	try
	{
		std::shared_ptr<detail::usb_request_context> ctx(new detail::usb_request_context());
		return ctx->control_read(m_core->hFile.get(), bmRequestType, bRequest, wValue, wIndex, buffer, size).follow_with([ctx](size_t){});
	}
	catch (...)
	{
		return async::raise<size_t>();
	}
}

task<void> usb_device::control_write(uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint8_t const * buffer, size_t size)
{
	try
	{
		std::shared_ptr<detail::usb_request_context> ctx(new detail::usb_request_context());
		return ctx->control_write(m_core->hFile.get(), bmRequestType, bRequest, wValue, wIndex, buffer, size).follow_with([ctx](){});
	}
	catch (...)
	{
		return async::raise<void>();
	}
}

usb_interface const & usb_device_interface::descriptor() const
{
	return m_core->configs[m_config_index].interfaces[m_interface_index];
}

std::string usb_device_interface::name() const
{
	return m_core->interface_names[m_config_index][m_interface_index];
}

uint8_t usb_device_interface::config_value() const
{
	return m_core->configs[m_config_index].bConfigurationValue;
}

void usb_device_interface::clear()
{
	m_core.reset();
}
