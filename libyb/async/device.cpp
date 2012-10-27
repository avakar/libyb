#include "device.hpp"
using namespace yb;

device::receiver_registration device::register_receiver(packet_handler & r)
{
	m_receivers.push_front(&r);
	return m_receivers.begin();
}

void device::unregister_receiver(receiver_registration reg)
{
	m_receivers.erase(reg);
}

void device::handle_packet(packet const & p)
{
	for (std::list<packet_handler *>::const_iterator it = m_receivers.begin(); it != m_receivers.end();)
	{
		std::list<packet_handler *>::const_iterator next = std::next(it);
		(*it)->handle_packet(p);
		it = next;
	}
}
