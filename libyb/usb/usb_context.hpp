#ifndef LIBYB_USB_USB_CONTEXT_HPP
#define LIBYB_USB_USB_CONTEXT_HPP

#include "usb_device.hpp"
#include "../async/async_runner.hpp"
#include "../utils/noncopyable.hpp"
#include <vector>
#include <memory>

namespace yb {

class usb_context
	: noncopyable
{
public:
	explicit usb_context(async_runner & runner);
	~usb_context();

	task<void> run();

	std::vector<usb_device> get_device_list() const;

private:
	struct impl;
	std::unique_ptr<impl> m_pimpl;
};

} // namespace yb

#endif // LIBYB_USB_CONTEXT_HPP
