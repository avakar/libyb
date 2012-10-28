#include "stream_device.hpp"
#include "task.hpp"
using namespace yb;

stream_device::stream_device()
	: m_start_write(signal::create())
{
}

task<void> stream_device::write_loop(stream & s)
{
	return wait_for(m_start_write).then([this, &s] {
		return s.write_all(m_write_buffer.data(), m_write_buffer.size());
	}).then([this]() -> task<void> {
		m_write_buffer.swap(m_write_backlog);
		m_write_backlog.clear();
		if (!m_write_buffer.empty())
			m_start_write.fire();
		return async::value();
	});
}

task<void> stream_device::run(stream & s)
{
	try
	{
		task<void> read_task = loop<size_t>(s.read(m_read_buffer, sizeof m_read_buffer), [this, &s](size_t r, cancel_level_t cancelled) -> task<size_t> {
			m_parser.parse(*this, buffer_ref(m_read_buffer, r));
			if (cancelled)
				return nulltask;
			return s.read(m_read_buffer, sizeof m_read_buffer);
		});

		task<void> write_task = loop([this, &s](cancel_level_t cancelled) {
			return cancelled? nulltask: this->write_loop(s);
		});

		return std::move(read_task) | std::move(write_task);
	}
	catch (...)
	{
		return async::raise<void>();
	}
}

static void append_packet(std::vector<uint8_t> & out, packet const & p)
{
	assert(p.size() < 17);

	out.push_back(0x80);
	out.push_back((uint8_t)((p[0] << 4) | (p.size() - 1)));
	out.insert(out.end(), p.begin() + 1, p.end());
}

void stream_device::write_packet(packet const & p)
{
	if (m_write_buffer.empty())
	{
		append_packet(m_write_buffer, p);

		task<void> t = m_start_write.fire();
		assert(t.has_result());
		(void)t;
	}
	else
	{
		append_packet(m_write_backlog, p);
	}
}
