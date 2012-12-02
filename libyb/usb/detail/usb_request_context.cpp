#include "usb_request_context.hpp"
#include "../../utils/utf.hpp"
using namespace yb;
using namespace yb::detail;

task<size_t> usb_request_context::get_descriptor(HANDLE hFile, uint8_t desc_type, uint8_t desc_index, uint16_t langid, unsigned char * data, int length)
{
	req = libusb0_win32_request();
	req.timeout = 5000;
	req.descriptor.type = desc_type;
	req.descriptor.index = desc_index;
	req.descriptor.language_id = langid;
	return opctx.ioctl(hFile, LIBUSB_IOCTL_GET_DESCRIPTOR, &req, sizeof req, data, length);
}

uint16_t usb_request_context::get_default_langid(HANDLE hFile)
{
	uint8_t buf[4];
	size_t read = this->get_descriptor_sync(hFile, 3, 0, 0, buf, sizeof buf);
	if (read < 2 || buf[0] < read || buf[1] != 3 || read % 2 != 0)
		throw std::runtime_error("invalid string descriptor");

	return read == 2? 0: buf[2] | (buf[3] << 8);
}

size_t usb_request_context::get_descriptor_sync(HANDLE hFile, uint8_t desc_type, uint8_t desc_index, uint16_t langid, unsigned char * data, int length)
{
	req = libusb0_win32_request();
	req.timeout = 5000;
	req.descriptor.type = desc_type;
	req.descriptor.index = desc_index;
	req.descriptor.language_id = langid;
	return opctx.sync_ioctl(hFile, LIBUSB_IOCTL_GET_DESCRIPTOR, &req, sizeof req, data, length);
}

std::string usb_request_context::get_string_descriptor_sync(HANDLE hFile, uint8_t index, uint16_t langid)
{
	uint8_t buf[256];
	size_t len = this->get_descriptor_sync(hFile, 3, index, langid, buf, sizeof buf);
	if (len < 2 || len % 2 != 0 || buf[0] != len || buf[1] != 3)
		throw std::runtime_error("invalid string descriptor");
	return utf16le_to_utf8(buffer_ref(buf + 2, buf + len));
}

void usb_request_context::get_device_descriptor(HANDLE hFile, usb_device_descriptor & desc)
{
	req = libusb0_win32_request();
	req.timeout = 5000;
	req.descriptor.type = 1;
	req.descriptor.index = 0;
	req.descriptor.language_id = 0;

	size_t r = opctx.sync_ioctl(hFile, LIBUSB_IOCTL_GET_DESCRIPTOR, &req, sizeof req, reinterpret_cast<uint8_t *>(&desc), sizeof desc);
	if (r != sizeof desc)
		throw std::runtime_error("too short a response");
	if (desc.bLength != sizeof desc || desc.bDescriptorType != 1)
		throw std::runtime_error("invalid response");

	// FIXME: endianity
}

task<uint8_t> usb_request_context::get_configuration(HANDLE hFile)
{
	req = libusb0_win32_request();
	return opctx.ioctl(hFile, LIBUSB_IOCTL_GET_CONFIGURATION, &req, sizeof req, stack_buf, 1).then([this](size_t r) -> task<uint8_t> {
		return r != 1? async::raise<uint8_t>(std::runtime_error("invalid response")): async::value(stack_buf[0]);
	});
}

task<void> usb_request_context::set_configuration(HANDLE hFile, uint8_t config)
{
	req = libusb0_win32_request();
	req.timeout = 5000;
	req.configuration.configuration = config;
	return opctx.ioctl(hFile, LIBUSB_IOCTL_SET_CONFIGURATION, &req, sizeof req, 0, 0).then([this](size_t) -> task<void> {
		return async::value();
	});
}

task<size_t> usb_request_context::bulk_read(HANDLE hFile, usb_endpoint_t ep, uint8_t * buffer, size_t size)
{
	assert((ep & 0x80) != 0);

	req = libusb0_win32_request();
	req.timeout = 5000;
	req.endpoint.endpoint = ep;
	return opctx.ioctl(
		hFile,
		LIBUSB_IOCTL_INTERRUPT_OR_BULK_READ,
		&req,
		sizeof req,
		buffer,
		size);
}

task<size_t> usb_request_context::bulk_write(HANDLE hFile, usb_endpoint_t ep, uint8_t const * buffer, size_t size)
{
	assert((ep & 0x80) == 0);

	req = libusb0_win32_request();
	req.timeout = 5000;
	req.endpoint.endpoint = ep;
	return opctx.ioctl(
		hFile,
		LIBUSB_IOCTL_INTERRUPT_OR_BULK_WRITE,
		&req,
		sizeof req,
		const_cast<uint8_t *>(buffer),
		size);
}

task<size_t> usb_request_context::control_read(HANDLE hFile, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint8_t * buffer, size_t size)
{
	assert((bmRequestType & 0x80) != 0);
	assert((bmRequestType & 0x60) < (3<<5));
	assert((bmRequestType & 0x1f) < 3);

	req = libusb0_win32_request();
	req.timeout = 5000;
	req.vendor.type = (bmRequestType & 0x60) >> 5;
	req.vendor.recipient = (bmRequestType & 0x1f);
	req.vendor.bRequest = bRequest;
	req.vendor.wValue = wValue;
	req.vendor.wIndex = wIndex;

	return opctx.ioctl(
		hFile,
		LIBUSB_IOCTL_VENDOR_READ,
		&req,
		sizeof req,
		buffer,
		size);
}

task<void> usb_request_context::control_write(HANDLE hFile, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint8_t const * buffer, size_t size)
{
	assert((bmRequestType & 0x80) == 0);
	assert((bmRequestType & 0x60) < (3<<5));
	assert((bmRequestType & 0x1f) < 3);

	req = libusb0_win32_request();
	req.timeout = 5000;
	req.vendor.type = (bmRequestType & 0x60) >> 5;
	req.vendor.recipient = (bmRequestType & 0x1f);
	req.vendor.bRequest = bRequest;
	req.vendor.wValue = wValue;
	req.vendor.wIndex = wIndex;

	buf.resize(sizeof(libusb0_win32_request) + size);
	std::copy(reinterpret_cast<uint8_t const *>(&req), reinterpret_cast<uint8_t const *>(&req) + sizeof req, buf.begin());
	std::copy(buffer, buffer + size, buf.begin() + sizeof req);

	return opctx.ioctl(
		hFile,
		LIBUSB_IOCTL_VENDOR_WRITE,
		buf.data(),
		buf.size(),
		0,
		0).ignore_result();
}

bool usb_request_context::claim_interface(HANDLE hFile, uint8_t intfno)
{
	req = libusb0_win32_request();
	req.intf.interface_number = intfno;
	try
	{
		opctx.sync_ioctl(hFile, LIBUSB_IOCTL_CLAIM_INTERFACE, &req, sizeof req, 0, 0);
	}
	catch (...)
	{
		return false;
	}

	return true;
}

void usb_request_context::release_interface(HANDLE hFile, uint8_t intfno)
{
	req = libusb0_win32_request();
	req.intf.interface_number = intfno;

	try
	{
		opctx.sync_ioctl(hFile, LIBUSB_IOCTL_RELEASE_INTERFACE, &req, sizeof req, 0, 0);
	}
	catch (...)
	{
	}
}
