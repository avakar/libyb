#include "interface_guard.hpp"
#include "../async/sync_runner.hpp"
using namespace yb;

usb_interface_guard::usb_interface_guard()
	: m_dev(0), m_intfno(0)
{
}

usb_interface_guard::~usb_interface_guard()
{
	try_run(this->release());
}

void usb_interface_guard::attach(usb_device & dev, uint8_t intfno)
{
	assert(intfno);
	m_dev = &dev;
	m_intfno = intfno;
}

void usb_interface_guard::detach()
{
	m_intfno = 0;
	m_dev = 0;
}

usb_device & usb_interface_guard::device() const
{
	assert(m_dev);
	return *m_dev;
}

uint8_t usb_interface_guard::intfno() const
{
	return m_intfno;
}

task<void> usb_interface_guard::claim(usb_device & dev, uint8_t intfno)
{
	assert(intfno);
	return dev.claim_interface(intfno).then([this]() -> task<void> {
		return this->release();
	}).then([this, &dev, intfno]() -> task<void> {
		this->attach(dev, intfno);
		return async::value();
	});
}

task<void> usb_interface_guard::release()
{
	if (!m_dev)
		return async::value();

	return m_dev->release_interface(m_intfno).then([this]() -> task<void> {
		this->detach();
		return async::value();
	});
}
