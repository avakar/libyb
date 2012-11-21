#ifndef LIBYB_USB_DETAIL_USB_REQUEST_CONTEXT_HPP
#define LIBYB_USB_DETAIL_USB_REQUEST_CONTEXT_HPP

#include "libusb0_win32_intf.h"
#include "../usb_descriptors.hpp"
#include "../../async/task.hpp"
#include "../../utils/detail/win32_file_operation.hpp"
#include <vector>
#include <stdint.h>
#include <windows.h>

namespace yb {
namespace detail {

class usb_request_context
{
public:
	task<size_t> get_descriptor(HANDLE hFile, uint8_t desc_type, uint8_t desc_index, uint16_t langid, unsigned char * data, int length);
	task<void> get_device_descriptor(HANDLE hFile, usb_device_descriptor & desc);

	task<uint8_t> get_configuration(HANDLE hFile);
	task<void> set_configuration(HANDLE hFile, uint8_t config);

	task<size_t> bulk_read(HANDLE hFile, usb_endpoint_t ep, uint8_t * buffer, size_t size);
	task<size_t> bulk_write(HANDLE hFile, usb_endpoint_t ep, uint8_t const * buffer, size_t size);

	task<size_t> control_read(HANDLE hFile, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint8_t * buffer, size_t size);
	task<void> control_write(HANDLE hFile, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint8_t const * buffer, size_t size);

	task<void> claim_interface(HANDLE hFile, uint8_t intfno);
	task<void> release_interface(HANDLE hFile, uint8_t intfno);

private:
	libusb0_win32_request req;
	uint8_t stack_buf[2];
	std::vector<uint8_t> buf;
	detail::win32_file_operation opctx;
};

} // namespace detail
} // namespace yb

#endif // LIBYB_USB_DETAIL_USB_REQUEST_CONTEXT_HPP
