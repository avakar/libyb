#include "usb_descriptors.hpp"
#include <cassert>
#include <stdexcept>
using namespace yb;

usb_config_descriptor yb::parse_config_descriptor(yb::buffer_ref d)
{
	assert(d.size() >= usb_raw_config_descriptor::size);

	usb_config_descriptor res;

	memcpy(&res, d.data(), usb_raw_config_descriptor::size);
	// FIXME: endianity

	d += usb_raw_config_descriptor::size;

	res.interfaces.resize(res.bNumInterfaces);

	usb_interface_descriptor * current_altsetting = 0;
	while (!d.empty())
	{
		uint8_t desclen = d[0];
		if (desclen > d.size() || desclen < 2)
			throw std::runtime_error("invalid descriptor");

		if (d[1] == 4/*INTERFACE*/)
		{
			if (desclen != usb_raw_interface_descriptor::size)
				throw std::runtime_error("invalid descriptor");

			usb_interface_descriptor idesc;
			memcpy(&idesc, d.data(), usb_raw_interface_descriptor::size);
			// FIXME: endianity

			if (idesc.bInterfaceNumber >= res.bNumInterfaces)
				throw std::runtime_error("invalid descriptor");

			usb_interface & intf = res.interfaces[idesc.bInterfaceNumber];
			if (intf.altsettings.size() != idesc.bAlternateSetting)
				throw std::runtime_error("invalid descriptor");

			intf.altsettings.push_back(idesc);
			current_altsetting = &intf.altsettings.back();
		}
		else if (d[1] == 5/*ENDPOINT*/)
		{
			if (!current_altsetting
				|| desclen != usb_raw_endpoint_descriptor::size)
			{
				throw std::runtime_error("invalid descriptor");
			}

			usb_endpoint_descriptor edesc;
			memcpy(&edesc, d.data(), usb_raw_endpoint_descriptor::size);
			// FIXME: endianity

			current_altsetting->endpoints.push_back(edesc);
		}
		else if (current_altsetting)
		{
			current_altsetting->extra_descriptors.push_back(std::vector<uint8_t>(d.data(), d.data() + desclen));
		}

		d += desclen;
	}

	// TODO: verify that each interface has a consistent number of endpoints.

	return res;
}

size_t usb_interface_descriptor::out_descriptor_count() const
{
	size_t res = 0;
	for (size_t i = 0; i < endpoints.size(); ++i)
	{
		if ((endpoints[i].bEndpointAddress & 0x80) == 0)
			++res;
	}
	return res;
}

size_t usb_interface_descriptor::in_descriptor_count() const
{
	size_t res = 0;
	for (size_t i = 0; i < endpoints.size(); ++i)
	{
		if ((endpoints[i].bEndpointAddress & 0x80) != 0)
			++res;
	}
	return res;
}
