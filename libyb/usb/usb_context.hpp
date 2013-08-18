#ifndef LIBYB_USB_USB_CONTEXT_HPP
#define LIBYB_USB_USB_CONTEXT_HPP

#include "usb_device.hpp"
#include "../async/runner.hpp"
#include "../utils/noncopyable.hpp"
#include <vector>
#include <memory>
#include <functional>

namespace yb {

struct usb_plugin_event
{
	enum action_t
	{
		a_initial = (1<<0),
		a_add = (1<<1),
		a_remove = (1<<2),

		a_all = a_initial | a_add | a_remove
	};

	action_t action;
	usb_device dev;
	usb_device_interface intf;

	usb_plugin_event()
	{
	}

	usb_plugin_event(action_t action, usb_device dev)
		: action(action), dev(dev)
	{
	}

	usb_plugin_event(action_t action, usb_device_interface intf)
		: action(action), intf(intf)
	{
	}
};

class usb_monitor;

class usb_context
	: noncopyable
{
public:
	explicit usb_context(runner & r);
	~usb_context();

	std::vector<usb_device> list_devices();
	usb_monitor monitor(
		std::function<void (usb_plugin_event const &)> const & event_sink,
		int action_mask = usb_plugin_event::a_add | usb_plugin_event::a_remove);

private:
	friend class usb_monitor;
	struct impl;
	std::unique_ptr<impl> m_pimpl;
};

class usb_monitor
	: noncopyable
{
public:
	usb_monitor(usb_monitor && o);
	~usb_monitor();

private:
	struct impl;
	std::unique_ptr<impl> m_pimpl;

	friend class usb_context;
	explicit usb_monitor(std::unique_ptr<impl> && pimpl);
};

} // namespace yb

#endif // LIBYB_USB_CONTEXT_HPP
