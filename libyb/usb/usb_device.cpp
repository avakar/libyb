#include "usb_device.hpp"
#include "../utils/tuple_less.hpp"
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

namespace yb {
	bool operator==(usb_device const & lhs, usb_device const & rhs)
	{
		return lhs.m_core == rhs.m_core;
	}

	bool operator!=(usb_device const & lhs, usb_device const & rhs)
	{
		return lhs.m_core != rhs.m_core;
	}

	bool operator<(usb_device const & lhs, usb_device const & rhs)
	{
		return lhs.m_core < rhs.m_core;
	}

	bool operator>(usb_device const & lhs, usb_device const & rhs)
	{
		return rhs.m_core < lhs.m_core;
	}

	bool operator<=(usb_device const & lhs, usb_device const & rhs)
	{
		return !(rhs.m_core < lhs.m_core);
	}

	bool operator>=(usb_device const & lhs, usb_device const & rhs)
	{
		return !(lhs.m_core < rhs.m_core);
	}
};

std::shared_ptr<detail::usb_device_core> const & usb_device::core() const
{
	return m_core;
}

usb_device_interface::usb_device_interface()
{
}

bool usb_device_interface::empty() const
{
	return m_core.get() == 0;
}

usb_device_interface::usb_device_interface(std::shared_ptr<detail::usb_device_core> const & core, size_t config_index, size_t interface_index)
	: m_core(core), m_config_index(config_index), m_interface_index(interface_index)
{
}

namespace yb
{
	bool operator==(usb_device_interface const & lhs, usb_device_interface const & rhs)
	{
		return lhs.m_core == rhs.m_core && (!lhs.m_core || (lhs.m_config_index == rhs.m_config_index && lhs.m_interface_index == rhs.m_interface_index));
	}

	bool operator!=(usb_device_interface const & lhs, usb_device_interface const & rhs)
	{
		return !(lhs == rhs);
	}

	bool operator<(usb_device_interface const & lhs, usb_device_interface const & rhs)
	{
		if (!lhs.m_core && !rhs.m_core)
			return false;
		return tuple_less(lhs.m_core, rhs.m_core)(lhs.m_config_index, rhs.m_config_index)(lhs.m_interface_index, rhs.m_interface_index);
	}

	bool operator>(usb_device_interface const & lhs, usb_device_interface const & rhs)
	{
		return rhs < lhs;
	}

	bool operator<=(usb_device_interface const & lhs, usb_device_interface const & rhs)
	{
		return !(rhs < lhs);
	}

	bool operator>=(usb_device_interface const & lhs, usb_device_interface const & rhs)
	{
		return !(lhs < rhs);
	}
};
