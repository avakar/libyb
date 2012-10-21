#ifndef LIBYB_PACKET_HANDLER_HPP
#define LIBYB_PACKET_HANDLER_HPP

#include "packet.hpp"

namespace yb {

class packet_handler
{
public:
	virtual void handle_packet(packet const & p)
	{
		this->handle_packet(packet(p));
	}

	virtual void handle_packet(packet && p)
	{
		this->handle_packet(p);
	}
};

} // namespace yb

#endif // LIBYB_PACKET_HANDLER_HPP
