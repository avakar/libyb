#ifndef LIBYB_USB_USB_H
#define LIBYB_USB_USB_H

#include "../utils/noncopyable.hpp"
#include "../async/task.hpp"
#include <vector>
#include <memory>
#include <stdint.h>

namespace yb {

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

typedef uint8_t usb_endpoint_t;

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

struct usb_device_core;
class usb_device;

class usb_context
	: noncopyable
{
public:
	usb_context();
	~usb_context();

	std::vector<usb_device> get_device_list() const;

private:
	struct impl;
	std::unique_ptr<impl> m_pimpl;
};

class usb_device
{
public:
	usb_device();
	~usb_device();

	usb_device_descriptor descriptor() const;
	usb_config_descriptor get_config_descriptor() const;

	std::vector<uint16_t> get_langid_list();
	std::string get_string_descriptor(uint8_t index, uint16_t langid);

	task<void> claim_interface(uint8_t intfno);
	task<void> release_interface(uint8_t intfno);

	task<size_t> bulk_read(usb_endpoint_t ep, uint8_t * buffer, size_t size);
	task<size_t> bulk_write(usb_endpoint_t ep, uint8_t const * buffer, size_t size);

	task<size_t> control_read(uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint8_t * buffer, size_t size);
	task<size_t> control_write(uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint8_t const * buffer, size_t size);

private:
	usb_device(std::shared_ptr<usb_device_core> core);
	std::shared_ptr<usb_device_core> m_core;
	friend class usb_context;
};

}

#endif // LIBYB_USB_USB_H
