#ifndef LIBYB_USB_DETAIL_USB_DEVICE_CORE_HPP
#define LIBYB_USB_DETAIL_USB_DEVICE_CORE_HPP

#include "usb_device_core_fwd.hpp"
#include "../usb_descriptors.hpp"
#include "../../utils/detail/scoped_unix_fd.hpp"
#include <vector>
#include <stddef.h>

namespace yb {
namespace detail {

struct usb_device_core
{
	scoped_unix_fd fd;
	usb_device_descriptor desc;
	std::vector<usb_config_descriptor> config_descs;
};

} // namespace detail
} // namespace yb

#endif // LIBYB_USB_DETAIL_USB_DEVICE_CORE_HPP
