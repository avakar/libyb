#include "../usb_context.hpp"
using namespace yb;

struct usb_context::impl
{
};

usb_context::usb_context()
	: m_pimpl(new impl())
{
}

usb_context::~usb_context()
{
}

std::vector<usb_device> usb_context::get_device_list() const
{
	// XXX
	return std::vector<usb_device>();
}
