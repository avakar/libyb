#ifndef LIBYB_USB_DESCRIPTOR_HPP
#define LIBYB_USB_DESCRIPTOR_HPP

#include "../vector_ref.hpp"
#include <vector>
#include <stdint.h>
#include <stddef.h>

namespace yb {

typedef uint8_t usb_endpoint_t;

struct usb_control_code_t
{
	uint8_t bmRequestType;
	uint8_t bRequest;
};

struct usb_device_descriptor
{
	uint8_t  bLength;
	uint8_t  bDescriptorType;
	uint16_t bcdUSB;
	uint8_t  bDeviceClass;
	uint8_t  bDeviceSubClass;
	uint8_t  bDeviceProtocol;
	uint8_t  bMaxPacketSize0;
	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t bcdDevice;
	uint8_t  iManufacturer;
	uint8_t  iProduct;
	uint8_t  iSerialNumber;
	uint8_t  bNumConfigurations;
};

struct usb_raw_endpoint_descriptor
{
	static size_t const size = 7;

	uint8_t  bLength;
	uint8_t  bDescriptorType;
	uint8_t  bEndpointAddress;
	uint8_t  bmAttributes;
	uint16_t wMaxPacketSize;
	uint8_t  bInterval;

	bool is_input() const
	{
		return (bEndpointAddress & 0x80) != 0;
	}

	bool is_output() const
	{
		return (bEndpointAddress & 0x80) == 0;
	}
};

struct usb_raw_interface_descriptor
{
	static size_t const size = 9;

	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bInterfaceNumber;
	uint8_t bAlternateSetting;
	uint8_t bNumEndpoints;
	uint8_t bInterfaceClass;
	uint8_t bInterfaceSubClass;
	uint8_t bInterfaceProtocol;
	uint8_t iInterface;
};

struct usb_raw_config_descriptor
{
	static size_t const size = 9;

	uint8_t  bLength;
	uint8_t  bDescriptorType;
	uint16_t wTotalLength;
	uint8_t  bNumInterfaces;
	uint8_t  bConfigurationValue;
	uint8_t  iConfiguration;
	uint8_t  bmAttributes;
	uint8_t  MaxPower;
};

typedef usb_raw_endpoint_descriptor usb_endpoint_descriptor;

struct usb_interface_descriptor
	: usb_raw_interface_descriptor
{
	std::vector<usb_endpoint_descriptor> endpoints;
};

struct usb_interface
{
	std::vector<usb_interface_descriptor> altsettings;
};

struct usb_config_descriptor
	: usb_raw_config_descriptor
{
	std::vector<usb_interface> interfaces;
};

usb_config_descriptor parse_config_descriptor(yb::buffer_ref d);

} // namespace yb

#endif // LIBYB_USB_DESCRIPTOR_HPP
