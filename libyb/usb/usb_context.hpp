#ifndef LIBYB_USB_USB_CONTEXT_HPP
#define LIBYB_USB_USB_CONTEXT_HPP

#include "usb_device.hpp"
#include "../async/async_runner.hpp"
#include "../utils/noncopyable.hpp"
#include <vector>
#include <memory>
#include <functional>

namespace yb {

struct usb_plugin_event
{
	enum action_t { a_add, a_remove };

	action_t action;
	usb_device dev;
	usb_device_interface intf;
};

class usb_context
	: noncopyable
{
public:
	explicit usb_context(async_runner & runner);
	~usb_context();

	async_future<void> run(std::function<void (usb_plugin_event const &)> const & event_sink);

	void get_device_list(std::vector<usb_device> & devs, std::vector<usb_device_interface> & intfs) const;

private:
	struct impl;
	std::unique_ptr<impl> m_pimpl;
};

} // namespace yb

#endif // LIBYB_USB_CONTEXT_HPP
