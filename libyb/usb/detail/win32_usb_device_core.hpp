#ifndef LIBYB_USB_DETAIL_WIN32_USB_DEVICE_CORE_HPP
#define LIBYB_USB_DETAIL_WIN32_USB_DEVICE_CORE_HPP

#include "usb_device_core_fwd.hpp"
#include "../usb_descriptors.hpp"
#include "../../utils/detail/scoped_win32_handle.hpp"

namespace yb {
namespace detail {

struct usb_device_core
{
	scoped_win32_handle hFile;
	usb_device_descriptor desc;
	std::vector<usb_config_descriptor> configs;
	uint16_t selected_langid;
	int active_config;
	size_t active_config_index;
	std::string product;
	std::string manufacturer;
	std::string serial_number;
	std::vector<std::vector<std::string>> interface_names;
};

} // namespace detail
} // namespace yb

#endif // LIBYB_USB_DETAIL_WIN32_USB_DEVICE_CORE_HPP
