#ifndef LIBYB_ASYNC_DEVICE_HPP
#define LIBYB_ASYNC_DEVICE_HPP

#include "../packet.hpp"
#include "../packet_handler.hpp"
#include <list>

namespace yb {

class device
	: protected packet_handler
{
public:
	typedef std::list<packet_handler *>::iterator receiver_registration;

	virtual ~device() {}
	virtual void write_packet(packet const & p) = 0;

	receiver_registration register_receiver(packet_handler * r);
	void unregister_receiver(receiver_registration reg);

protected:
	void handle_packet(packet const & p) const;

private:
	std::list<packet_handler *> m_receivers;
};

} // namespace yb

#endif // LIBYB_ASYNC_DEVICE_HPP
