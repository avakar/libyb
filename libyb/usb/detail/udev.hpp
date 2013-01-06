#ifndef LIBYB_USB_DETAIL_UDEV_HPP
#define LIBYB_USB_DETAIL_UDEV_HPP

#include <stdexcept>
#include <libudev.h>

namespace yb {
namespace detail {

struct scoped_udev
{
public:
	explicit scoped_udev(struct udev * ctx = 0)
		: m_ctx(ctx)
	{
	}

	scoped_udev(scoped_udev const & o)
		: m_ctx(o.m_ctx)
	{
		if (m_ctx)
			udev_ref(m_ctx);
	}

	scoped_udev & operator=(scoped_udev const & o)
	{
		if (o.m_ctx)
			udev_ref(o.m_ctx);
		if (m_ctx)
			udev_unref(m_ctx);
		m_ctx = o.m_ctx;
		return *this;
	}

	~scoped_udev()
	{
		if (m_ctx)
			udev_unref(m_ctx);
	}

	bool empty() const
	{
		return m_ctx == 0;
	}

	void reset(struct udev * ctx = 0)
	{
		if (m_ctx)
			udev_unref(m_ctx);
		m_ctx = ctx;
	}

	struct udev * get() const
	{
		return m_ctx;
	}

private:
	struct udev * m_ctx;
};

struct scoped_udev_enumerate
{
public:
	explicit scoped_udev_enumerate(struct udev_enumerate * ctx)
		: m_ctx(ctx)
	{
		if (!ctx)
			throw std::runtime_error("failed to create udev enumerate");
	}

	scoped_udev_enumerate(scoped_udev_enumerate const & o)
		: m_ctx(o.m_ctx)
	{
		udev_enumerate_ref(m_ctx);
	}

	scoped_udev_enumerate & operator=(scoped_udev_enumerate const & o)
	{
		udev_enumerate_ref(o.m_ctx);
		udev_enumerate_unref(m_ctx);
		m_ctx = o.m_ctx;
		return *this;
	}

	~scoped_udev_enumerate()
	{
		udev_enumerate_unref(m_ctx);
	}

	operator struct udev_enumerate *() const
	{
		return m_ctx;
	}

private:
	struct udev_enumerate * m_ctx;
};

struct scoped_udev_device
{
public:
	explicit scoped_udev_device(struct udev_device * ctx = 0)
		: m_ctx(ctx)
	{
	}

	scoped_udev_device(scoped_udev_device const & o)
		: m_ctx(o.m_ctx)
	{
		if (m_ctx)
			udev_device_ref(m_ctx);
	}

	scoped_udev_device & operator=(scoped_udev_device const & o)
	{
		if (o.m_ctx)
			udev_device_ref(o.m_ctx);
		if (m_ctx)
			udev_device_unref(m_ctx);
		m_ctx = o.m_ctx;
		return *this;
	}

	~scoped_udev_device()
	{
		if (m_ctx)
			udev_device_unref(m_ctx);
	}

	bool empty() const
	{
		return m_ctx == 0;
	}

	struct udev_device * get() const
	{
		return m_ctx;
	}

private:
	struct udev_device * m_ctx;
};

struct scoped_udev_monitor
{
	explicit scoped_udev_monitor(struct udev_monitor * ctx = 0)
		: m_ctx(ctx)
	{
	}

	scoped_udev_monitor(scoped_udev_monitor const & o)
		: m_ctx(o.m_ctx)
	{
		if (m_ctx)
			udev_monitor_ref(m_ctx);
	}

	scoped_udev_monitor & operator=(scoped_udev_monitor const & o)
	{
		if (o.m_ctx)
			udev_monitor_ref(o.m_ctx);
		if (m_ctx)
			udev_monitor_unref(m_ctx);
		m_ctx = o.m_ctx;
		return *this;
	}

	~scoped_udev_monitor()
	{
		if (m_ctx)
			udev_monitor_unref(m_ctx);
	}

	bool empty() const
	{
		return m_ctx == 0;
	}

	void reset(struct udev_monitor * v)
	{
		if (m_ctx)
			udev_monitor_unref(m_ctx);
		m_ctx = v;
	}

	struct udev_monitor * get() const
	{
		return m_ctx;
	}

private:
	struct udev_monitor * m_ctx;
};

inline void udev_check_error(int r)
{
	if (r != 0)
		throw std::runtime_error("udev error");
}

} // namespace detail
} // namespace yb

#endif // LIBYB_USB_DETAIL_UDEV_HPP
