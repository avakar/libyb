#include "usb_device.hpp"
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

task<size_t> usb_device::control_read(usb_control_code_t const & code, uint16_t wValue, uint16_t wIndex, uint8_t * buffer, size_t size)
{
	return this->control_read(code.bmRequestType, code.bRequest, wValue, wIndex, buffer, size);
}

task<void> usb_device::control_write(usb_control_code_t const & code, uint16_t wValue, uint16_t wIndex, uint8_t const * buffer, size_t size)
{
	return this->control_write(code.bmRequestType, code.bRequest, wValue, wIndex, buffer, size);
}

bool yb::operator==(usb_device const & lhs, usb_device const & rhs)
{
	return lhs.m_core == rhs.m_core;
}

bool yb::operator!=(usb_device const & lhs, usb_device const & rhs)
{
	return lhs.m_core != rhs.m_core;
}

bool yb::operator<(usb_device const & lhs, usb_device const & rhs)
{
	return lhs.m_core < rhs.m_core;
}

bool yb::operator>(usb_device const & lhs, usb_device const & rhs)
{
	return rhs.m_core < lhs.m_core;
}

bool yb::operator<=(usb_device const & lhs, usb_device const & rhs)
{
	return !(rhs.m_core < lhs.m_core);
}

bool yb::operator>=(usb_device const & lhs, usb_device const & rhs)
{
	return !(lhs.m_core < rhs.m_core);
}
