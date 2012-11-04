#include "tunnel.hpp"
#include "async/promise.hpp"
using namespace yb;

tunnel_handler::tunnel_handler()
	: m_dev(0)
{
}

tunnel_handler::~tunnel_handler()
{
	if (m_dev)
		m_dev->unregister_receiver(m_reg);
}

bool tunnel_handler::attach(device & dev, device_descriptor const & desc)
{
	if (device_config const * config = desc.get_config("356e9bf7-8718-4965-94a4-0be370c8797c"))
		m_config = *config;
	else
		return false;

	m_dev = &dev;
	m_reg = m_dev->register_receiver(*this);
	return true;
}

task<void> tunnel_handler::request_tunnel_list()
{
	return m_dev->write_packet(make_packet(m_config.cmd) % 0 % 0);
}

task<tunnel_handler::tunnel_list_t> tunnel_handler::list_tunnels()
{
	try
	{
		promise<tunnel_list_t> ch;
		m_on_tunnel_list.oneshot([ch](tunnel_list_t const & tl) { ch.set_value(tl); });
		this->request_tunnel_list();
		return wait_for(ch);
	}
	catch (...)
	{
		return async::raise<tunnel_list_t>();
	}
}

void tunnel_handler::handle_packet(packet const & p)
{
	if (p[0] != m_config.cmd)
		return;

	if (p.size() >= 3 && p[1] == 0 && p[2] == 0)
	{
		tunnel_list_t tl = tunnel_handler::parse_tunnel_list(p);
		m_on_tunnel_list.broadcast(tl);
	}
	else if (p.size() == 4 && p[1] == 0 && p[2] == 1)
	{
		if (m_active_opens.empty())
			return;

		promise<uint8_t> current_open = m_active_opens.front();
		m_active_opens.pop_front();

		uint8_t pipe_no = p[3];
		current_open.set_value(pipe_no);
	}
	else if (p.size() > 2)
	{
		std::map<uint8_t, std::deque<read_irp>>::iterator it = m_read_irps.find(p[1]);
		if (it != m_read_irps.end() && !it->second.empty())
		{
			read_irp r = it->second.front();
			it->second.pop_front();

			size_t chunk = (std::min)(r.size, p.size() - 2);
			std::copy(p.begin() + 2, p.begin() + 2 + chunk, r.buffer);
			r.transferred.set_value(chunk);
		}
	}
}

tunnel_handler::tunnel_list_t tunnel_handler::parse_tunnel_list(packet const & p)
{
	std::vector<std::string> pipe_names;
	for (size_t i = 3; i < p.size();)
	{
		if (i + 1 + p[i] > p.size())
			throw std::runtime_error("Invalid response while enumerating pipes.");

		std::string pipe_name;
		for (size_t j = 0; j < p[i]; ++j)
			pipe_name.push_back(p[i+j+1]);
		pipe_names.push_back(pipe_name);

		i += 1 + p[i];
	}
	return pipe_names;
}

task<uint8_t> tunnel_handler::open(string_ref const & name)
{
	// FIXME: exception safety
	promise<uint8_t> p;
	m_active_opens.push_back(p);

	return m_dev->write_packet(yb::make_packet(m_config.cmd) % 0 % 1 % name).then([p] {
		return wait_for(p);
	});
}

task<void> tunnel_handler::fast_close(uint8_t pipe_no)
{
	return m_dev->write_packet(yb::make_packet(m_config.cmd) % 0 % 2 % pipe_no);
}

task<size_t> tunnel_handler::read(uint8_t pipe_no, uint8_t * buffer, size_t size)
{
	read_irp irp = { buffer, size };
	m_read_irps[pipe_no].push_back(irp);
	return wait_for(irp.transferred);
}

task<size_t> tunnel_handler::write(uint8_t pipe_no, uint8_t const * buffer, size_t size)
{
	size_t chunk = (std::min)(size, (size_t)14);
	return m_dev->write_packet(make_packet(m_config.cmd) % pipe_no % buffer_ref(buffer, chunk)).then([chunk] {
		return async::value((size_t)chunk);
	});
}

tunnel_stream::tunnel_stream()
	: m_th(0), m_pipe_no(0)
{
}

tunnel_stream::~tunnel_stream()
{
}

task<void> tunnel_stream::open(tunnel_handler & th, string_ref const & name)
{
	return th.open(name).then([this, &th](uint8_t pipe_no) {
		m_th = &th;
		m_pipe_no = pipe_no;
	});
}

task<void> tunnel_stream::fast_close()
{
	task<void> res;

	if (m_th)
	{
		res = m_th->fast_close(m_pipe_no);
		m_th = 0;
	}

	return std::move(res);
}

task<size_t> tunnel_stream::read(uint8_t * buffer, size_t size)
{
	return m_th->read(m_pipe_no, buffer, size);
}

task<size_t> tunnel_stream::write(uint8_t const * buffer, size_t size)
{
	return m_th->write(m_pipe_no, buffer, size);
}
