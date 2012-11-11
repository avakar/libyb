#ifndef LIBYB_USB_INTERFACE_GUARD_HPP
#define LIBYB_USB_INTERFACE_GUARD_HPP

#include "usb_device.hpp"
#include "../utils/noncopyable.hpp"

namespace yb {

class usb_interface_guard
	: noncopyable
{
public:
	usb_interface_guard();
	~usb_interface_guard();

	void attach(usb_device & dev, uint8_t intfno);
	void detach();

	usb_device & device() const;
	uint8_t intfno() const;

	task<void> claim(usb_device & dev, uint8_t intfno);
	task<void> release();

private:
	usb_device * m_dev;
	uint8_t m_intfno;
};

}

#endif // LIBYB_USB_INTERFACE_GUARD_HPP
