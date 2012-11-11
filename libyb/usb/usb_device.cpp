#include "usb_device.hpp"
#include "detail/usb_device_core.hpp"
#include "detail/usb_request_context.hpp"
#include "../async/sync_runner.hpp"
#include "../async/detail/win32_handle_task.hpp"
#include "../utils/utf.hpp"
using namespace yb;

usb_device::usb_device()
{
}

usb_device::usb_device(std::shared_ptr<detail::usb_device_core> core)
	: m_core(core)
{
	assert(core.get() != nullptr);
}

usb_device::~usb_device()
{
}

void usb_device::clear()
{
	m_core.reset();
}

bool usb_device::empty() const
{
	return m_core.get() == nullptr;
}

usb_device_descriptor usb_device::descriptor() const
{
	return m_core->desc;
}

usb_config_descriptor usb_device::get_config_descriptor() const
{
	detail::usb_request_context ctx;

	uint8_t desc_header[4];
	if (run(ctx.get_descriptor(m_core->hFile.get(), 2, 0, 0, desc_header, sizeof desc_header)) != sizeof desc_header)
		throw std::runtime_error("request failure");

	uint16_t wTotalLength = desc_header[2] | (desc_header[3] << 8);
	if (wTotalLength < usb_raw_config_descriptor::size)
		throw std::runtime_error("invalid config descriptor");

	std::vector<uint8_t> desc(wTotalLength);

	for (;;)
	{
		size_t len = run(ctx.get_descriptor(m_core->hFile.get(), 2, 0, 0, desc.data(), desc.size()));
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

std::vector<uint16_t> usb_device::get_langid_list()
{
	detail::usb_request_context ctx;

	uint8_t buf[256];
	size_t read = run(ctx.get_descriptor(m_core->hFile.get(), 3, 0, 0, buf, sizeof buf));
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

	uint8_t buf[256];
	size_t read = run(ctx.get_descriptor(m_core->hFile.get(), 3, index, langid, buf, sizeof buf));
	if (read < 2 || buf[0] != read || buf[1] != 3)
		throw std::runtime_error("invalid string descriptor");

	return utf16le_to_utf8(yb::buffer_ref(buf + 2, read - 2));
}

task<void> usb_device::claim_interface(uint8_t intfno)
{
	try
	{
		std::shared_ptr<detail::usb_request_context> ctx(new detail::usb_request_context());
		return ctx->claim_interface(m_core->hFile.get(), intfno).follow_with([ctx](){});
	}
	catch (...)
	{
		return async::raise<void>();
	}
}

task<void> usb_device::release_interface(uint8_t intfno)
{
	try
	{
		std::shared_ptr<detail::usb_request_context> ctx(new detail::usb_request_context());
		return ctx->release_interface(m_core->hFile.get(), intfno).follow_with([ctx](){});
	}
	catch (...)
	{
		return async::raise<void>();
	}
}

task<size_t> usb_device::bulk_read(usb_endpoint_t ep, uint8_t * buffer, size_t size)
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

task<size_t> usb_device::bulk_write(usb_endpoint_t ep, uint8_t const * buffer, size_t size)
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

task<size_t> usb_device::control_read(usb_control_code_t const & code, uint16_t wValue, uint16_t wIndex, uint8_t * buffer, size_t size)
{
	return this->control_read(code.bmRequestType, code.bRequest, wValue, wIndex, buffer, size);
}

task<void> usb_device::control_write(usb_control_code_t const & code, uint16_t wValue, uint16_t wIndex, uint8_t const * buffer, size_t size)
{
	return this->control_write(code.bmRequestType, code.bRequest, wValue, wIndex, buffer, size);
}
