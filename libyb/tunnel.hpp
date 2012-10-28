#ifndef LIBYB_TUNNEL_HPP
#define LIBYB_TUNNEL_HPP

#include "async/task.hpp"
#include "async/stream.hpp"
#include "async/device.hpp"
#include "async/channel.hpp"
#include "descriptor.hpp"

namespace yb {

class tunnel_handler
	: private packet_handler
{
public:
	typedef std::vector<std::string> tunnel_list_t;

	tunnel_handler();
	~tunnel_handler();

	bool attach(device & dev, device_descriptor const & desc);

	task<tunnel_list_t> list_tunnels();

private:
	void handle_packet(packet const & p);
	static tunnel_list_t parse_tunnel_list(packet const & p);

	device * m_dev;
	device::receiver_registration m_reg;
	device_config m_config;

	channel<tunnel_list_t> m_tunnel_list_channel;
};

} // namespace yb

#endif // LIBYB_TUNNEL_HPP
