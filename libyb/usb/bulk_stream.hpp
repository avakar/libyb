#ifndef LIBYB_USB_BULK_STREAM_HPP
#define LIBYB_USB_BULK_STREAM_HPP

#include "../async/stream.hpp"
#include "interface_guard.hpp"
#include "usb.h"

namespace yb {

class usb_bulk_stream
	: public stream
{
public:
	usb_bulk_stream();

	bool open(usb_device & dev, usb_endpoint_t read_ep, usb_endpoint_t write_ep);
	task<void> claim_and_open(usb_device & dev, usb_interface_guard & g, usb_interface_descriptor const & idesc);
	task<void> claim_and_open(usb_device & dev, usb_interface_guard & g, usb_config_descriptor const & cdesc);

	task<void> close();

	bool is_open() const;
	bool is_readable() const;
	bool is_writable() const;

	task<size_t> read(uint8_t * buffer, size_t size);
	task<size_t> write(uint8_t const * buffer, size_t size);

private:
	usb_device * m_dev;
	uint8_t m_claimed_intf;
	usb_endpoint_t m_read_ep;
	usb_endpoint_t m_write_ep;
};

} // namespace yb

#endif // LIBYB_USB_BULK_STREAM_HPP
