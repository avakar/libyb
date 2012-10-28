#include "tunnel.hpp"
using namespace yb;

tunnel_handler::tunnel_handler()
	: m_dev(0), m_tunnel_list_channel(channel<tunnel_list_t>::create())
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

task<tunnel_handler::tunnel_list_t> tunnel_handler::list_tunnels()
{
	m_dev->write_packet(make_packet(m_config.cmd) % 0 % 0);
	return m_tunnel_list_channel.receive();
}

void tunnel_handler::handle_packet(packet const & p)
{
	if (p[0] != m_config.cmd)
		return;

	if (p.size() >= 3 && p[1] == 0 && p[2] == 0)
	{
		m_tunnel_list_channel.send(tunnel_handler::parse_tunnel_list(p));
		m_tunnel_list_channel = channel<tunnel_list_t>::create();
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
