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

	size_t get_config_descriptor_count() const;
	usb_config_descriptor get_config_descriptor(size_t index) const;
	usb_config_descriptor get_config_descriptor_by_value(uint8_t value) const;
	usb_config_descriptor get_config_descriptor() const;

	uint32_t vidpid() const;

	uint16_t get_default_langid() const;
	std::vector<uint16_t> get_langid_list();
	std::string get_string_descriptor(uint8_t index, uint16_t langid);

	std::string product() const;
	std::string manufacturer() const;
	std::string serial_number() const;

	uint8_t get_cached_configuration() const;
	task<uint8_t> get_configuration();
	task<void> set_configuration(uint8_t config);

	bool claim_interface(uint8_t intfno);
	void release_interface(uint8_t intfno);

	task<size_t> bulk_read(usb_endpoint_t ep, uint8_t * buffer, size_t size) const;
	task<size_t> bulk_write(usb_endpoint_t ep, uint8_t const * buffer, size_t size) const;

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
