#include "stream_device.hpp"
#include "task.hpp"
using namespace yb;

task<void> stream_device::write_loop(stream & s)
{
	return (m_write_buffer.empty()? wait_for(m_start_write): make_value_task()).then([this, &s] {
		return s.write_all(m_write_buffer.data(), m_write_buffer.size());
	}).then([this]() -> task<void> {
		m_write_buffer.swap(m_write_backlog);
		m_write_backlog.clear();
		return make_value_task();
	});
}

task<void> stream_device::run(stream & s)
{
	try
	{
		task<void> read_task = loop<size_t>(s.read(m_read_buffer, sizeof m_read_buffer), [this, &s](size_t r) -> task<size_t> {
			m_parser.parse(*this, buffer_ref(m_read_buffer, r));
			return s.read(m_read_buffer, sizeof m_read_buffer);
		});

		task<void> write_task = loop([this, &s] {
			return this->write_loop(s);
		});

		return std::move(read_task) | std::move(write_task);
	}
	catch (...)
	{
		return make_value_task<void>(std::current_exception());
	}
}

static void append_packet(std::vector<uint8_t> & out, packet const & p)
{
	out.push_back(0x80);
	out.push_back((p[0] << 4) | (p.size() - 1));
	out.insert(out.end(), p.begin() + 1, p.end());
}

void stream_device::write_packet(packet const & p)
{
	if (m_write_buffer.empty())
	{
		append_packet(m_write_buffer, p);
		m_start_write.fire();
	}
	else
	{
		append_packet(m_write_backlog, p);
	}
}
