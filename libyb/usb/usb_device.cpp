#include "usb.h"
#include "../utils/noncopyable.hpp"
#include "detail/libusb0_win32_intf.h"
#include "../async/task.hpp"
#include "../async/detail/win32_handle_task.hpp"
#include "../async/sync_runner.hpp"
#include "../vector_ref.hpp"
#include "../utils/utf.hpp"
#include <map>
#include <memory>
#include <Windows.h>
using namespace yb;

class scoped_win32_handle
	: noncopyable
{
public:
	explicit scoped_win32_handle(HANDLE handle = INVALID_HANDLE_VALUE)
		: m_handle(handle)
	{
	}

	scoped_win32_handle(scoped_win32_handle && o)
		: m_handle(o.m_handle)
	{
		o.m_handle = INVALID_HANDLE_VALUE;
	}

	~scoped_win32_handle()
	{
		if (this->empty())
			CloseHandle(m_handle);
	}

	scoped_win32_handle & operator=(scoped_win32_handle o)
	{
		m_handle = o.m_handle;
		o.m_handle = INVALID_HANDLE_VALUE;
		return *this;
	}

	bool empty() const
	{
		return m_handle == INVALID_HANDLE_VALUE;
	}

	HANDLE get() const
	{
		return m_handle;
	}

	HANDLE release()
	{
		HANDLE handle = m_handle;
		m_handle = INVALID_HANDLE_VALUE;
		return handle;
	}

private:
	HANDLE m_handle;
};

struct win32_overlapped
{
	win32_overlapped()
		: o()
	{
		o.hEvent = CreateEvent(0, TRUE, FALSE, 0);
		if (!o.hEvent)
			throw std::runtime_error("couldn't create event");
	}

	~win32_overlapped()
	{
		CloseHandle(o.hEvent);
	}

	OVERLAPPED o;
};

static bool cancel_win32_irp(cancel_level cl, HANDLE hFile, OVERLAPPED * o)
{
	if (cl < cl_abort)
		return true;

	if (HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll"))
	{
		typedef BOOL WINAPI CancelIoEx_t(HANDLE hFile, LPOVERLAPPED lpOverlapped);
		if (CancelIoEx_t * CancelIoEx = (CancelIoEx_t *)GetProcAddress(hKernel32, "CancelIoEx"))
			CancelIoEx(hFile, o);
		else
			CancelIo(hFile);
	}
	else
	{
		CancelIo(hFile);
	}

	return true;
}

class win32_file_operation
{
public:
	task<size_t> ioctl(HANDLE hFile, DWORD dwControlCode, void const * in_data, size_t in_len, void * out_data, size_t out_len)
	{
		DWORD dwTransferred;
		if (!DeviceIoControl(hFile, dwControlCode, (void *)in_data, in_len, out_data, out_len, &dwTransferred, &m_overlapped.o))
		{
			DWORD dwError = GetLastError();
			if (dwError == ERROR_IO_PENDING)
			{
				return make_win32_handle_task(m_overlapped.o.hEvent, [this, hFile](cancel_level cl) {
					return cancel_win32_irp(cl, hFile, &m_overlapped.o);
				}).then([this, hFile]() -> task<size_t> {
					DWORD dwTransferred;
					if (!GetOverlappedResult(hFile, &m_overlapped.o, &dwTransferred, TRUE))
						return async::raise<size_t>(std::runtime_error("GetOverlappedResult failure"));
					return async::value((size_t)dwTransferred);
				});
			}
			else
			{
				return async::raise<size_t>(std::runtime_error("DeviceIoControl failed"));
			}
		}
		else
		{
			return async::value((size_t)dwTransferred);
		}
	}

private:
	win32_overlapped m_overlapped;
};

class usb_request_context
{
public:
	task<size_t> get_descriptor(HANDLE hFile, uint8_t desc_type, uint8_t desc_index, uint16_t langid, unsigned char * data, int length)
	{
		req = libusb0_win32_request();
		req.timeout = 5000;
		req.descriptor.type = desc_type;
		req.descriptor.index = desc_index;
		req.descriptor.language_id = langid;
		return opctx.ioctl(hFile, LIBUSB_IOCTL_GET_DESCRIPTOR, &req, sizeof req, data, length);
	}

	task<void> get_device_descriptor(HANDLE hFile, usb_device_descriptor & desc)
	{
		return this->get_descriptor(hFile, 1, 0, 0, reinterpret_cast<uint8_t *>(&desc), sizeof desc).then([&desc](size_t r) -> task<void> {
			if (r != sizeof desc)
				return async::raise<void>(std::runtime_error("too short a response"));
			if (desc.bLength != sizeof desc || desc.bDescriptorType != 1)
				return async::raise<void>(std::runtime_error("invalid response"));
			// FIXME: endianity
			return async::value();
		});
	}

	task<size_t> bulk_read(HANDLE hFile, usb_endpoint_t ep, uint8_t * buffer, size_t size)
	{
		assert((ep & 0x80) != 0);

		req = libusb0_win32_request();
		req.timeout = 5000;
		req.endpoint.endpoint = ep;
		return opctx.ioctl(
			hFile,
			LIBUSB_IOCTL_INTERRUPT_OR_BULK_READ,
			&req,
			sizeof req,
			buffer,
			size);
	}

	task<size_t> bulk_write(HANDLE hFile, usb_endpoint_t ep, uint8_t const * buffer, size_t size)
	{
		assert((ep & 0x80) == 0);

		req = libusb0_win32_request();
		req.timeout = 5000;
		req.endpoint.endpoint = ep;
		return opctx.ioctl(
			hFile,
			LIBUSB_IOCTL_INTERRUPT_OR_BULK_WRITE,
			&req,
			sizeof req,
			const_cast<uint8_t *>(buffer),
			size);
	}

	task<size_t> control_read(HANDLE hFile, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint8_t * buffer, size_t size)
	{
		assert((bmRequestType & 0x80) != 0);
		assert((bmRequestType & 0x60) < (3<<5));
		assert((bmRequestType & 0x1f) < 3);

		req = libusb0_win32_request();
		req.timeout = 5000;
		req.vendor.type = (bmRequestType & 0x60) >> 5;
		req.vendor.recipient = (bmRequestType & 0x1f);
		req.vendor.bRequest = bRequest;
		req.vendor.wValue = wValue;
		req.vendor.wIndex = wIndex;

		return opctx.ioctl(
			hFile,
			LIBUSB_IOCTL_VENDOR_READ,
			&req,
			sizeof req,
			buffer,
			size);
	}

	task<size_t> control_write(HANDLE hFile, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint8_t const * buffer, size_t size)
	{
		assert((bmRequestType & 0x80) == 0);
		assert((bmRequestType & 0x60) < (3<<5));
		assert((bmRequestType & 0x1f) < 3);

		req = libusb0_win32_request();
		req.timeout = 5000;
		req.vendor.type = (bmRequestType & 0x60) >> 5;
		req.vendor.recipient = (bmRequestType & 0x1f);
		req.vendor.bRequest = bRequest;
		req.vendor.wValue = wValue;
		req.vendor.wIndex = wIndex;

		buf.resize(sizeof(libusb0_win32_request) + size);
		std::copy(reinterpret_cast<uint8_t const *>(&req), reinterpret_cast<uint8_t const *>(&req) + sizeof req, buf.begin());
		std::copy(buffer, buffer + size, buf.begin() + sizeof req);

		return opctx.ioctl(
			hFile,
			LIBUSB_IOCTL_VENDOR_WRITE,
			buf.data(),
			buf.size(),
			0,
			0);
	}

	task<void> claim_interface(HANDLE hFile, uint8_t intfno)
	{
		req = libusb0_win32_request();
		req.intf.interface_number = intfno;
		return opctx.ioctl(hFile, LIBUSB_IOCTL_CLAIM_INTERFACE, &req, sizeof req, 0, 0).ignore_result();
	}

	task<void> release_interface(HANDLE hFile, uint8_t intfno)
	{
		req = libusb0_win32_request();
		req.intf.interface_number = intfno;
		return opctx.ioctl(hFile, LIBUSB_IOCTL_RELEASE_INTERFACE, &req, sizeof req, 0, 0).ignore_result();
	}

private:
	libusb0_win32_request req;
	std::vector<uint8_t> buf;
	win32_file_operation opctx;
};

struct yb::usb_device_core
{
	scoped_win32_handle hFile;
	usb_device_descriptor desc;
};

struct usb_context::impl
{
	std::map<size_t, std::weak_ptr<usb_device_core>> m_device_repository;
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
	std::vector<usb_device> res;
	res.reserve(m_pimpl->m_device_repository.size());

	usb_request_context get_descriptor_ctx;
	for (size_t i = 1; i < LIBUSB_MAX_NUMBER_OF_DEVICES; ++i)
	{
		WCHAR devname[32];
		wsprintf(devname, L"\\\\.\\libusb0-%04d", i);

		scoped_win32_handle hFile(CreateFileW(devname, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL));
		if (hFile.empty())
			continue;

		std::weak_ptr<usb_device_core> & repo_dev = m_pimpl->m_device_repository[i];
		if (std::shared_ptr<usb_device_core> dev = repo_dev.lock())
		{
			res.push_back(std::move(dev));
			continue;
		}

		usb_device_descriptor desc;
		if (try_run(get_descriptor_ctx.get_device_descriptor(hFile.get(), desc)).has_exception())
			continue;

		std::shared_ptr<usb_device_core> dev(new usb_device_core());
		dev->hFile = std::move(hFile);
		dev->desc = desc;
		repo_dev = dev;
		res.push_back(std::move(dev));
	}

	return res;
}

usb_device::usb_device()
{
}

usb_device::usb_device(std::shared_ptr<usb_device_core> core)
	: m_core(core)
{
	assert(core.get() != nullptr);
}

usb_device::~usb_device()
{
}

usb_device_descriptor usb_device::descriptor() const
{
	return m_core->desc;
}

static usb_config_descriptor parse_config_descriptor(yb::buffer_ref d)
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

		if (d[1] == 5/*ENDPOINT*/)
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

		d += desclen;
	}

	// TODO: verify that each interface has a consistent number of endpoints.

	return res;
}

usb_config_descriptor usb_device::get_config_descriptor() const
{
	usb_request_context ctx;

	uint8_t desc_header[4];
	if (run(ctx.get_descriptor(m_core->hFile.get(), 2, 0, 0, desc_header, sizeof desc_header)) != sizeof desc_header)
		throw std::runtime_error("request failure");

	uint16_t wTotalLength = desc_header[2] | (desc_header[3] << 8);
	if (wTotalLength < usb_raw_config_descriptor::size)
		throw std::runtime_error("invalid config descriptor");

	std::vector<uint8_t> desc(wTotalLength);

	for (;;)
	{
		size_t len = run(ctx.get_descriptor(m_core->hFile.get(), 2, 0, 0, desc.data(), desc.size()));
		if (len < usb_raw_config_descriptor::size)
			throw std::runtime_error("invalid config descriptor");

		wTotalLength = desc[2] | (desc[3] << 8);
		if (wTotalLength < usb_raw_config_descriptor::size)
			throw std::runtime_error("invalid config descriptor");

		if (desc.size() >= wTotalLength)
		{
			if (len < wTotalLength)
				throw std::runtime_error("invalid config descriptor");

			desc.resize(wTotalLength);
			break;
		}

		desc.resize(wTotalLength);
	}

	return parse_config_descriptor(desc);
}

std::vector<uint16_t> usb_device::get_langid_list()
{
	usb_request_context ctx;

	uint8_t buf[256];
	size_t read = run(ctx.get_descriptor(m_core->hFile.get(), 3, 0, 0, buf, sizeof buf));
	if (read < 2 || buf[0] != read || buf[1] != 3 || read % 2 != 0)
		throw std::runtime_error("invalid string descriptor");

	std::vector<uint16_t> res;
	for (size_t i = 2; i < read; i += 2)
		res.push_back(buf[i] | (buf[i+1] << 8));
	return res;
}

std::string usb_device::get_string_descriptor(uint8_t index, uint16_t langid)
{
	usb_request_context ctx;

	uint8_t buf[256];
	size_t read = run(ctx.get_descriptor(m_core->hFile.get(), 3, index, langid, buf, sizeof buf));
	if (read < 2 || buf[0] != read || buf[1] != 3)
		throw std::runtime_error("invalid string descriptor");

	return utf16le_to_utf8(yb::buffer_ref(buf + 2, read - 2));
}

task<void> usb_device::claim_interface(uint8_t intfno)
{
	try
	{
		std::shared_ptr<usb_request_context> ctx(new usb_request_context());
		return ctx->claim_interface(m_core->hFile.get(), intfno).follow_with([ctx](){});
	}
	catch (...)
	{
		return async::raise<void>();
	}
}

task<void> usb_device::release_interface(uint8_t intfno)
{
	try
	{
		std::shared_ptr<usb_request_context> ctx(new usb_request_context());
		return ctx->release_interface(m_core->hFile.get(), intfno).follow_with([ctx](){});
	}
	catch (...)
	{
		return async::raise<void>();
	}
}

task<size_t> usb_device::bulk_read(usb_endpoint_t ep, uint8_t * buffer, size_t size)
{
	try
	{
		std::shared_ptr<usb_request_context> ctx(new usb_request_context());
		return ctx->bulk_read(m_core->hFile.get(), ep, buffer, size).follow_with([ctx](size_t){});
	}
	catch (...)
	{
		return async::raise<size_t>();
	}
}

task<size_t> usb_device::bulk_write(usb_endpoint_t ep, uint8_t const * buffer, size_t size)
{
	try
	{
		std::shared_ptr<usb_request_context> ctx(new usb_request_context());
		return ctx->bulk_write(m_core->hFile.get(), ep, buffer, size).follow_with([ctx](size_t){});
	}
	catch (...)
	{
		return async::raise<size_t>();
	}
}

task<size_t> usb_device::control_read(uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint8_t * buffer, size_t size)
{
	try
	{
		std::shared_ptr<usb_request_context> ctx(new usb_request_context());
		return ctx->control_read(m_core->hFile.get(), bmRequestType, bRequest, wValue, wIndex, buffer, size).follow_with([ctx](size_t){});
	}
	catch (...)
	{
		return async::raise<size_t>();
	}
}

task<size_t> usb_device::control_write(uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint8_t const * buffer, size_t size)
{
	try
	{
		std::shared_ptr<usb_request_context> ctx(new usb_request_context());
		return ctx->control_write(m_core->hFile.get(), bmRequestType, bRequest, wValue, wIndex, buffer, size).follow_with([ctx](size_t){});
	}
	catch (...)
	{
		return async::raise<size_t>();
	}
}
