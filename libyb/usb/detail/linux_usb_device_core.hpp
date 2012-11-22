#ifndef LIBYB_USB_DETAIL_LINUX_USB_DEVICE_CORE_HPP
#define LIBYB_USB_DETAIL_LINUX_USB_DEVICE_CORE_HPP

#include "usb_device_core_fwd.hpp"
#include "../usb_descriptors.hpp"

namespace yb {
namespace detail {

struct usb_device_core
{
	usb_device_descriptor desc;
};

} // namespace detail
} // namespace yb

#endif // LIBYB_USB_DETAIL_LINUX_USB_DEVICE_CORE_HPP
