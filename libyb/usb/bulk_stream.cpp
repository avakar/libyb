#include "bulk_stream.hpp"
#include <stdexcept>
using namespace yb;

usb_bulk_stream::usb_bulk_stream()
	: m_dev(0), m_read_ep(0), m_write_ep(0)
{
}

bool usb_bulk_stream::open(usb_device & dev, usb_endpoint_t read_ep, usb_endpoint_t write_ep)
{
	if ((!read_ep && !write_ep)
		|| (read_ep != 0 && (read_ep & 0x80) == 0)
		|| (write_ep & 0x80) != 0)
	{
		return false;
	}

	m_dev = &dev;
	m_read_ep = read_ep;
	m_write_ep = write_ep;
	return true;
}

bool usb_bulk_stream::claim_and_open(usb_device & dev, usb_interface_guard & g, usb_interface_descriptor const & idesc)
{
	usb_endpoint_t read_ep = 0;
	usb_endpoint_t write_ep = 0;

	for (size_t i = 0; i < idesc.endpoints.size(); ++i)
	{
		if (idesc.endpoints[i].is_input())
		{
			if (read_ep)
				return false;
			read_ep = idesc.endpoints[i].bEndpointAddress;
		}

		if (idesc.endpoints[i].is_output())
		{
			if (write_ep)
				return false;
			write_ep = idesc.endpoints[i].bEndpointAddress;
		}
	}

	if (!g.claim(dev, idesc.bInterfaceNumber))
		return false;

	m_dev = &dev;
	m_read_ep = read_ep;
	m_write_ep = write_ep;
	return true;
}

bool usb_bulk_stream::claim_and_open(usb_device & dev, usb_interface_guard & g, usb_config_descriptor const & cdesc)
{
	yb::usb_interface_descriptor const * selected_idesc = 0;
	for (size_t i = 0; !selected_idesc && i < cdesc.interfaces.size(); ++i)
	{
		yb::usb_interface const & intf = cdesc.interfaces[i];
		for (size_t j = 0; !selected_idesc && j < intf.altsettings.size(); ++j)
		{
			yb::usb_interface_descriptor const & idesc = intf.altsettings[j];
			if (idesc.bInterfaceClass == 0xa)
				selected_idesc = &idesc;
		}
	}

	if (!selected_idesc)
		return false;

	return this->claim_and_open(dev, g, *selected_idesc);
}

void usb_bulk_stream::close()
{
	if (!m_dev)
		return;

	if (m_claimed_intf != 0)
		m_dev->release_interface(m_claimed_intf);

	m_dev->clear();
	m_claimed_intf = 0;
	m_read_ep = 0;
	m_write_ep = 0;
}

bool usb_bulk_stream::is_open() const
{
	return m_dev != 0;
}

bool usb_bulk_stream::is_readable() const
{
	return m_dev != 0 && m_read_ep != 0;
}

bool usb_bulk_stream::is_writable() const
{
	return m_dev != 0 && m_write_ep != 0;
}

task<size_t> usb_bulk_stream::read(uint8_t * buffer, size_t size)
{
	assert(m_dev && m_read_ep);
	return m_dev->bulk_read(m_read_ep, buffer, size);
}

task<size_t> usb_bulk_stream::write(uint8_t const * buffer, size_t size)
{
	assert(m_dev && m_write_ep);
	return m_dev->bulk_write(m_write_ep, buffer, size);
}
