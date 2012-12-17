#include "interface_guard.hpp"
#include "../async/sync_runner.hpp"
using namespace yb;

usb_interface_guard::usb_interface_guard()
	: m_intfno(0)
{
}

usb_interface_guard::~usb_interface_guard()
{
	this->release();
}

void usb_interface_guard::attach(usb_device const & dev, uint8_t intfno)
{
	assert(!dev.empty());
	m_dev = dev;
	m_intfno = intfno;
}

void usb_interface_guard::detach()
{
	m_dev.clear();
}

usb_device usb_interface_guard::device() const
{
	assert(!m_dev.empty());
	return m_dev;
}

uint8_t usb_interface_guard::intfno() const
{
	return m_intfno;
}

bool usb_interface_guard::claim(usb_device const & dev, uint8_t intfno)
{
	assert(!dev.empty());
	if (!dev.claim_interface(intfno))
		return false;

	this->release();
	this->attach(dev, intfno);
	return true;
}

void usb_interface_guard::release()
{
	if (m_dev.empty())
		return;

	m_dev.release_interface(m_intfno);
	this->detach();
}
