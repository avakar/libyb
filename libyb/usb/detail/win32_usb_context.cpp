#include "../usb_context.hpp"
#include "win32_usb_device_core.hpp"
#include "usb_request_context.hpp"
#include "../../async/async_runner.hpp"
#include "../../async/timer.hpp"
#include "../../utils/detail/win32_mutex.hpp"
#include <map>
#include <list>
#include <array>
#include <memory>
using namespace yb;

struct usb_context::impl
	: noncopyable
{
	struct monitor_entry
	{
		std::function<void (usb_plugin_event const &)> event_sink;
		int action_mask;
	};

	runner & m_runner;

	std::array<std::shared_ptr<detail::usb_device_core>, LIBUSB_MAX_NUMBER_OF_DEVICES> m_devices;
	std::map<size_t, std::weak_ptr<detail::usb_device_core>> m_device_repository;

	timer m_refresh_timer;
	task<void> m_refresh_loop;

	impl(runner & r)
		: m_runner(r)
	{
	}

	task<void> run_one();

	detail::win32_mutex m_mutex;
	std::list<monitor_entry> m_monitors;

	void monitor_current(usb_plugin_event::action_t action, std::function<void (usb_plugin_event const &)> const & event_sink);
	void refresh_device_list();
	void emit_events(usb_plugin_event const & ev);
};

struct usb_monitor::impl
{
	usb_context::impl * ctx_pimpl;
	std::list<usb_context::impl::monitor_entry>::iterator me_iter;
};

usb_monitor::usb_monitor(std::unique_ptr<impl> && pimpl)
	: m_pimpl(std::move(pimpl))
{
}

usb_monitor::usb_monitor(usb_monitor && o)
	: m_pimpl(std::move(o.m_pimpl))
{
}

usb_monitor::~usb_monitor()
{
	if (m_pimpl.get())
	{
		detail::scoped_win32_lock l(m_pimpl->ctx_pimpl->m_mutex);
		m_pimpl->ctx_pimpl->m_monitors.erase(m_pimpl->me_iter);
	}
}

usb_context::usb_context(runner & r)
	: m_pimpl(new impl(r))
{
	m_pimpl->refresh_device_list();
	m_pimpl->m_refresh_loop = m_pimpl->m_runner.post(yb::loop([this](cancel_level cl) -> task<void> {
		return cl >= cl_quit? nulltask: m_pimpl->run_one();
	}));
}

usb_context::~usb_context()
{
}

std::vector<usb_device> usb_context::list_devices()
{
	std::vector<usb_device> res;

	{
		detail::scoped_win32_lock l(m_pimpl->m_mutex);
		for (size_t i = 0; i < m_pimpl->m_devices.size(); ++i)
		{
			if (m_pimpl->m_devices[i].get())
				res.push_back(usb_device(m_pimpl->m_devices[i]));
		}
	}

	return res;
}

usb_monitor usb_context::monitor(
	std::function<void (usb_plugin_event const &)> const & event_sink,
	int action_mask)
{
	std::unique_ptr<usb_monitor::impl> pimpl(new usb_monitor::impl());
	pimpl->ctx_pimpl = m_pimpl.get();

	detail::scoped_win32_lock l(m_pimpl->m_mutex);

	impl::monitor_entry me;
	me.event_sink = event_sink;
	me.action_mask = action_mask;
	m_pimpl->m_monitors.push_back(std::move(me));
	pimpl->me_iter = std::prev(m_pimpl->m_monitors.end());

	usb_monitor res(std::move(pimpl));

	if (action_mask & usb_plugin_event::a_initial)
		m_pimpl->monitor_current(usb_plugin_event::a_initial, event_sink);

	return std::move(res);
}

task<void> usb_context::impl::run_one()
{
	return m_refresh_timer.wait_ms(1000).then([this] {
		detail::scoped_win32_lock l(m_mutex);
		this->refresh_device_list();
	});
}

void usb_context::impl::monitor_current(usb_plugin_event::action_t action, std::function<void (usb_plugin_event const &)> const & event_sink)
{
	for (size_t i = 0; i < m_devices.size(); ++i)
	{
		std::shared_ptr<detail::usb_device_core> const & core = m_devices[i];
		if (core.get() == nullptr)
			continue;

		event_sink(usb_plugin_event(usb_plugin_event::a_initial, usb_device(core)));

		if (core->active_config_index < core->configs.size())
		{
			usb_config_descriptor * config = &core->configs[core->active_config_index];
			for (size_t i = 0; i < config->interfaces.size(); ++i)
				event_sink(usb_plugin_event(action, usb_device_interface(core, core->active_config_index, i)));
		}
	}
}

void usb_context::impl::refresh_device_list()
{
	std::vector<usb_device> res;
	std::vector<usb_device_interface> intf_res;
	res.reserve(m_device_repository.size());
	intf_res.reserve(res.capacity());

	detail::usb_request_context get_descriptor_ctx;
	for (size_t i = 1; i < LIBUSB_MAX_NUMBER_OF_DEVICES; ++i)
	{
		WCHAR devname[32];
		wsprintf(devname, L"\\\\.\\libusb0-%04d", i);

		detail::scoped_win32_handle hFile(CreateFileW(devname, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL));
		if (hFile.empty())
		{
			if (std::shared_ptr<detail::usb_device_core> core = m_devices[i])
			{
				m_devices[i].reset();

				if (core->active_config_index < core->configs.size())
				{
					usb_config_descriptor * config = &core->configs[core->active_config_index];
					for (size_t i = 0; i < config->interfaces.size(); ++i)
						this->emit_events(usb_plugin_event(usb_plugin_event::a_remove, usb_device_interface(core, core->active_config_index, i)));
				}

				this->emit_events(usb_plugin_event(usb_plugin_event::a_remove, usb_device(core)));
			}
			continue;
		}

		std::weak_ptr<detail::usb_device_core> & repo_dev = m_device_repository[i];
		if (std::shared_ptr<detail::usb_device_core> dev = repo_dev.lock())
			continue;

		std::shared_ptr<detail::usb_device_core> dev(new detail::usb_device_core());
		try
		{
			get_descriptor_ctx.get_device_descriptor(hFile.get(), dev->desc);

			bool invalid_config_desc = false;
			for (uint8_t i = 0; i < dev->desc.bNumConfigurations; ++i)
			{
				uint8_t config_desc_header[4];
				size_t r = get_descriptor_ctx.get_descriptor_sync(hFile.get(), 2, i, 0, config_desc_header, sizeof config_desc_header);
				if (r < 4)
				{
					invalid_config_desc = true;
					break;
				}

				uint16_t wTotalLength = config_desc_header[2] | (config_desc_header[3] << 8);

				std::vector<uint8_t> config_desc(wTotalLength);
				r = get_descriptor_ctx.get_descriptor_sync(hFile.get(), 2, i, 0, config_desc.data(), config_desc.size());
				if (r != wTotalLength)
				{
					invalid_config_desc = true;
					break;
				}

				dev->configs.push_back(parse_config_descriptor(config_desc));
			}

			if (invalid_config_desc)
				continue;

			dev->active_config = -1;
			dev->active_config_index = dev->configs.size();

			dev->selected_langid = get_descriptor_ctx.get_default_langid(hFile.get());

			dev->interface_names.resize(dev->configs.size());
			for (size_t i = 0; i < dev->configs.size(); ++i)
			{
				dev->interface_names[i].resize(dev->configs[i].interfaces.size());
				for (size_t j = 0; j < dev->configs[i].interfaces.size(); ++j)
				{
					if (dev->configs[i].interfaces[j].altsettings.empty())
						continue;
					if (uint8_t iInterface = dev->configs[i].interfaces[j].altsettings[0].iInterface)
						dev->interface_names[i][j] = get_descriptor_ctx.get_string_descriptor_sync(hFile.get(), iInterface, dev->selected_langid);
				}
			}

			if (dev->desc.iProduct)
				dev->product = get_descriptor_ctx.get_string_descriptor_sync(hFile.get(), dev->desc.iProduct, dev->selected_langid);
			if (dev->desc.iManufacturer)
				dev->manufacturer = get_descriptor_ctx.get_string_descriptor_sync(hFile.get(), dev->desc.iManufacturer, dev->selected_langid);
			if (dev->desc.iSerialNumber)
				dev->serial_number = get_descriptor_ctx.get_string_descriptor_sync(hFile.get(), dev->desc.iSerialNumber, dev->selected_langid);

			dev->hFile = std::move(hFile);
		}
		catch (...)
		{
			continue;
		}

		repo_dev = dev;
		m_devices[i] = dev;
		this->emit_events(usb_plugin_event(usb_plugin_event::a_add, usb_device(dev)));
	}

	for (size_t i = 0; i < m_devices.size(); ++i)
	{
		if (!m_devices[i])
			continue;

		std::shared_ptr<detail::usb_device_core> core = m_devices[i];
		usb_device dev = usb_device(core);

		uint8_t active_config = dev.get_cached_configuration();
		if (core->active_config == active_config)
			continue;

		if (core->active_config_index < core->configs.size())
		{
			usb_config_descriptor * config = &core->configs[core->active_config_index];
			for (size_t i = 0; i < config->interfaces.size(); ++i)
				this->emit_events(usb_plugin_event(usb_plugin_event::a_remove, usb_device_interface(core, core->active_config_index, i)));
		}

		core->active_config = active_config;
		core->active_config_index = core->configs.size();
		for (size_t i = 0; i < core->configs.size(); ++i)
		{
			if (core->configs[i].bConfigurationValue == core->active_config)
			{
				core->active_config_index = i;
				break;
			}
		}

		if (core->active_config_index < core->configs.size())
		{
			usb_config_descriptor * config = &core->configs[core->active_config_index];
			for (size_t i = 0; i < config->interfaces.size(); ++i)
				this->emit_events(usb_plugin_event(usb_plugin_event::a_add, usb_device_interface(core, core->active_config_index, i)));
		}
	}
}

void usb_context::impl::emit_events(usb_plugin_event const & ev)
{
	for (std::list<monitor_entry>::const_iterator it = m_monitors.begin(), eit = m_monitors.end(); it != eit; ++it)
	{
		if (it->action_mask & ev.action)
			it->event_sink(ev);
	}
}
