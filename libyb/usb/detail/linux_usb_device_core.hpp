#ifndef LIBYB_USB_DETAIL_LINUX_USB_DEVICE_CORE_HPP
#define LIBYB_USB_DETAIL_LINUX_USB_DEVICE_CORE_HPP

#include "usb_device_core_fwd.hpp"
#include "udev.hpp"
#include "../usb_descriptors.hpp"
#include "../../async/async_runner.hpp"
#include "../../async/promise.hpp"
#include "../../utils/detail/scoped_unix_fd.hpp"
#include <string>
#include <linux/usbdevice_fs.h>

namespace yb {
namespace detail {

struct urb_context
{
	struct usbdevfs_urb urb;
	promise<void> done;
};

struct usb_device_core
{
	scoped_unix_fd fd;
	bool writable;
	scoped_udev udev;
	std::string syspath;
	usb_device_descriptor desc;
	std::vector<usb_config_descriptor> configs;
	async_future<void> dispatch_loop;

	std::string iProduct;
	std::string iManufacturer;
	std::string iSerialNumber;
};

} // namespace detail
} // namespace yb

#endif // LIBYB_USB_DETAIL_LINUX_USB_DEVICE_CORE_HPP
