#ifndef LIBYB_USB_USB_DEVICE_HPP
#define LIBYB_USB_USB_DEVICE_HPP

#include "usb_descriptors.hpp"
#include "detail/usb_device_core_fwd.hpp"
#include "../async/task.hpp"
#include <vector>
#include <string>
#include <memory>
#include <stdint.h>

namespace yb {

class usb_device
{
public:
	usb_device();
	explicit usb_device(std::shared_ptr<detail::usb_device_core> core);
	~usb_device();

	void clear();
	bool empty() const;

	usb_device_descriptor descriptor() const;
	usb_config_descriptor get_config_descriptor() const;

	std::vector<uint16_t> get_langid_list();
	std::string get_string_descriptor(uint8_t index, uint16_t langid);

	task<void> claim_interface(uint8_t intfno);
	task<void> release_interface(uint8_t intfno);

	task<size_t> bulk_read(usb_endpoint_t ep, uint8_t * buffer, size_t size);
	task<size_t> bulk_write(usb_endpoint_t ep, uint8_t const * buffer, size_t size);

	task<size_t> control_read(uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint8_t * buffer, size_t size);
	task<void> control_write(uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint8_t const * buffer, size_t size);

	task<size_t> control_read(usb_control_code_t const & code, uint16_t wValue, uint16_t wIndex, uint8_t * buffer, size_t size);
	task<void> control_write(usb_control_code_t const & code, uint16_t wValue, uint16_t wIndex, uint8_t const * buffer, size_t size);

	friend bool operator==(usb_device const & lhs, usb_device const & rhs);
	friend bool operator!=(usb_device const & lhs, usb_device const & rhs);
	friend bool operator<(usb_device const & lhs, usb_device const & rhs);
	friend bool operator>(usb_device const & lhs, usb_device const & rhs);
	friend bool operator<=(usb_device const & lhs, usb_device const & rhs);
	friend bool operator>=(usb_device const & lhs, usb_device const & rhs);

private:
	std::shared_ptr<detail::usb_device_core> m_core;
};

}

#endif // LIBYB_USB_DEVICE_HPP