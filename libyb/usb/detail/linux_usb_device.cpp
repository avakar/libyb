#include "../usb_device.hpp"
#include "linux_usb_device_core.hpp"
#include "../../utils/utf.hpp"
#include <stdexcept>
#include <stdio.h>
#include <sys/ioctl.h>
using namespace yb;

usb_device_descriptor usb_device::descriptor() const
{
	return m_core->desc;
}

size_t usb_device::get_config_descriptor_count() const
{
	assert(m_core);
	return m_core->configs.size();
}

usb_config_descriptor usb_device::get_config_descriptor(size_t index) const
{
	assert(m_core);
	assert(index < m_core->configs.size());
	return m_core->configs[index];
}

usb_config_descriptor usb_device::get_config_descriptor_by_value(uint8_t value) const
{
	assert(m_core);
	for (size_t i = 0; i < m_core->configs.size(); ++i)
	{
		if (m_core->configs[i].bConfigurationValue == value)
			return m_core->configs[i];
	}

	throw std::runtime_error("no matching config descriptor");
}

usb_config_descriptor usb_device::get_config_descriptor() const
{
	return this->get_config_descriptor_by_value(this->get_cached_configuration());
}

uint32_t usb_device::vidpid() const
{
	return ((uint32_t)m_core->desc.idVendor << 16) | m_core->desc.idProduct;
}

uint16_t usb_device::get_default_langid() const
{
	uint8_t buf[4];

	struct usbdevfs_ctrltransfer req;
	req.bRequestType = 0x80;
	req.bRequest = 6/*GET_DESCRIPTOR*/;
	req.wValue = 0x300;
	req.wIndex = 0;
	req.wLength = sizeof buf;
	req.data = buf;
	req.timeout = 5000; // XXX

	int r = ioctl(m_core->fd.get(), USBDEVFS_CONTROL, &req);
	if (r != 4 || buf[1] != 3 || buf[0] < 4)
		return 0;

	return buf[2] | (buf[3] << 8);
}

std::vector<uint16_t> usb_device::get_langid_list()
{
	uint8_t buf[256];

	struct usbdevfs_ctrltransfer req;
	req.bRequestType = 0x80;
	req.bRequest = 6/*GET_DESCRIPTOR*/;
	req.wValue = 0x300;
	req.wIndex = 0;
	req.wLength = sizeof buf;
	req.data = buf;
	req.timeout = 5000; // XXX

	int r = ioctl(m_core->fd.get(), USBDEVFS_CONTROL, &req);
	if (r == -1)
		throw std::runtime_error("failed to read dev descriptor");

	if (r < 2 || r % 2 != 0 || buf[0] != r || buf[1] != 3)
		throw std::runtime_error("invalid descriptor");

	std::vector<uint16_t> res(r / 2 - 1);
	for (size_t i = 0; i < res.size(); ++i)
		res[i] = buf[2*i+2] | (buf[2*i+3] << 8);
	return res;
}

std::string usb_device::get_string_descriptor(uint8_t index, uint16_t langid)
{
	uint8_t buf[256];

	struct usbdevfs_ctrltransfer req;
	req.bRequestType = 0x80;
	req.bRequest = 6/*GET_DESCRIPTOR*/;
	req.wValue = 0x300 | index;
	req.wIndex = langid;
	req.wLength = sizeof buf;
	req.data = buf;
	req.timeout = 5000; // XXX

	int r = ioctl(m_core->fd.get(), USBDEVFS_CONTROL, &req);
	if (r == -1)
		throw std::runtime_error("failed to read dev descriptor");

	if (r < 2 || buf[0] != r || buf[1] != 3)
		throw std::runtime_error("invalid descriptor");

	return utf16le_to_utf8(buffer_ref(buf + 2, buf + r));
}

std::string usb_device::product() const
{
	assert(m_core);
	return m_core->iProduct;
}

std::string usb_device::manufacturer() const
{
	assert(m_core);
	return m_core->iManufacturer;
}

std::string usb_device::serial_number() const
{
	assert(m_core);
	return m_core->iSerialNumber;
}

uint8_t usb_device::get_cached_configuration() const
{
	assert(m_core);
	detail::scoped_udev_device dev(udev_device_new_from_syspath(m_core->udev.get(), m_core->syspath.c_str()));
	if (dev.empty())
		throw std::runtime_error("cannot open the device");

	char const * config_value_str = udev_device_get_sysattr_value(dev.get(), "bConfigurationValue");
	if (!config_value_str)
		throw std::runtime_error("no config value in sysfs");
	return (uint8_t)atoi(config_value_str);
}

task<uint8_t> usb_device::get_configuration()
{
	std::shared_ptr<uint8_t> ctx(std::make_shared<uint8_t>(0));
	return this->control_read(0x80, 8/*GET_CONFIGURATION*/, 0, 0, ctx.get(), 1).then([ctx](size_t r) {
		return async::value((uint8_t)(r? *ctx: 0));
	});
}

task<void> usb_device::set_configuration(uint8_t config)
{
	assert(m_core);
	int ioctl_arg = config;
	if (ioctl(m_core->fd.get(), USBDEVFS_SETCONFIGURATION, &ioctl_arg) < 0)
		return async::raise<void>(std::runtime_error("failed to set configuration"));
	return async::value();
}

bool usb_device::claim_interface(uint8_t intfno) const
{
	assert(m_core);
	int ioctl_arg = intfno;
	if (ioctl(m_core->fd.get(), USBDEVFS_CLAIMINTERFACE, &ioctl_arg) < 0)
		return false;
	return true;
}

void usb_device::release_interface(uint8_t intfno) const
{
	assert(m_core);
	int ioctl_arg = intfno;
	ioctl(m_core->fd.get(), USBDEVFS_RELEASEINTERFACE, &ioctl_arg);
}

static task<size_t> async_transfer(std::shared_ptr<detail::usb_device_core> const & core,
	unsigned char type, usb_endpoint_t ep, void * buffer, size_t size, int flags)
{
	assert(core);

	return protect([&]() {
		std::shared_ptr<detail::urb_context> ctx(new detail::urb_context());

		struct usbdevfs_urb * urb = &ctx->urb;
		urb->type = type;
		urb->endpoint = ep;
		urb->buffer = buffer;
		urb->buffer_length = size;
		urb->usercontext = ctx.get();
		urb->flags = flags;

		std::set<detail::urb_context *>::iterator it = core->pending_urbs.insert(ctx.get()).first;
		if (ioctl(core->fd.get(), USBDEVFS_SUBMITURB, urb) < 0)
		{
			core->pending_urbs.erase(it);
			return async::raise<size_t>(std::runtime_error("cannot submit urb"));
		}

		return ctx->done.wait_for([core, ctx](cancel_level cl) -> bool {
			if (cl >= cl_abort)
				ioctl(core->fd.get(), USBDEVFS_DISCARDURB, &ctx->urb);
			return true;
		}).then([ctx]() -> task<size_t> {
			if (ctx->urb.status < 0)
				return async::raise<size_t>(std::runtime_error("transfer error"));
			return async::value((size_t)ctx->urb.actual_length);
		});
	});
}

task<size_t> usb_device::bulk_read(usb_endpoint_t ep, uint8_t * buffer, size_t size) const
{
	return async_transfer(m_core, USBDEVFS_URB_TYPE_BULK, ep, buffer, size, 0);
}

task<size_t> usb_device::bulk_write(usb_endpoint_t ep, uint8_t const * buffer, size_t size) const
{
	return async_transfer(m_core, USBDEVFS_URB_TYPE_BULK, ep, const_cast<uint8_t *>(buffer), size, 0);
}

task<size_t> usb_device::bulk_write_zlp(usb_endpoint_t ep, uint8_t const * buffer, size_t size, size_t /*epsize*/) const
{
	return async_transfer(m_core, USBDEVFS_URB_TYPE_BULK, ep, const_cast<uint8_t *>(buffer), size, USBDEVFS_URB_ZERO_PACKET);
}

task<size_t> usb_device::control_read(uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint8_t * buffer, size_t size)
{
	std::shared_ptr<std::vector<uint8_t> > ctx = std::make_shared<std::vector<uint8_t> >(size + 8);
	std::vector<uint8_t> & v = *ctx;
	v[0] = bmRequestType;
	v[1] = bRequest;
	v[2] = wValue;
	v[3] = wValue >> 8;
	v[4] = wIndex;
	v[5] = wIndex >> 8;
	v[6] = size;
	v[7] = size >> 8;

	return async_transfer(m_core, USBDEVFS_URB_TYPE_CONTROL, 0x80, v.data(), size + 8, 0).then([ctx, buffer](size_t r) {
		std::copy(ctx->begin() + 8, ctx->begin() + 8 + r, buffer);
		return async::value(r);
	});
}

task<void> usb_device::control_write(uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint8_t const * buffer, size_t size)
{
	std::shared_ptr<std::vector<uint8_t> > ctx = std::make_shared<std::vector<uint8_t> >(size + 8);
	std::vector<uint8_t> & v = *ctx;
	v[0] = bmRequestType;
	v[1] = bRequest;
	v[2] = wValue;
	v[3] = wValue >> 8;
	v[4] = wIndex;
	v[5] = wIndex >> 8;
	v[6] = size;
	v[7] = size >> 8;
	std::copy(buffer, buffer + size, v.data() + 8);

	return async_transfer(m_core, USBDEVFS_URB_TYPE_CONTROL, 0x00, v.data(), size + 8, 0).then([ctx](size_t) {});
}

usb_interface const & usb_device_interface::descriptor() const
{
	return m_core->configs[m_config_index].interfaces[m_interface_index];
}

std::string usb_device_interface::name() const
{
	return m_core->intfnames[m_config_index][m_interface_index];
}

uint8_t usb_device_interface::config_value() const
{
	return m_core->configs[m_config_index].bConfigurationValue;
}

void usb_device_interface::clear()
{
	m_core.reset();
}
