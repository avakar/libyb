#include "../usb_device.hpp"
#include "linux_usb_device_core.hpp"
#include <stdexcept>
using namespace yb;

usb_device_descriptor usb_device::descriptor() const
{
	return m_core->desc;
}

usb_config_descriptor usb_device::get_config_descriptor() const
{
	// XXX
	return usb_config_descriptor();
}

uint32_t usb_device::vidpid() const
{
	return ((uint32_t)m_core->desc.idVendor << 16) | m_core->desc.idProduct;
}

uint16_t usb_device::get_default_langid() const
{
	// XXX
	return 0;
}

std::vector<uint16_t> usb_device::get_langid_list()
{
	//XXX
	return std::vector<uint16_t>();
}

std::string usb_device::get_string_descriptor(uint8_t index, uint16_t langid)
{
	// XXX
	return "";
}

task<uint8_t> usb_device::get_configuration()
{
	throw std::runtime_error("XXX");
}

task<void> usb_device::set_configuration(uint8_t config)
{
	throw std::runtime_error("XXX");
}

task<void> usb_device::claim_interface(uint8_t intfno)
{
	throw std::runtime_error("XXX");
}

task<void> usb_device::release_interface(uint8_t intfno)
{
	throw std::runtime_error("XXX");
}

task<size_t> usb_device::bulk_read(usb_endpoint_t ep, uint8_t * buffer, size_t size) const
{
	throw std::runtime_error("XXX");
}

task<size_t> usb_device::bulk_write(usb_endpoint_t ep, uint8_t const * buffer, size_t size) const
{
	throw std::runtime_error("XXX");
}

task<size_t> usb_device::control_read(uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint8_t * buffer, size_t size)
{
	throw std::runtime_error("XXX");
}

task<void> usb_device::control_write(uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint8_t const * buffer, size_t size)
{
	throw std::runtime_error("XXX");
}
