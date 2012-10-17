#ifndef LIBYB_ASYNC_DEVICE_HPP
#define LIBYB_ASYNC_DEVICE_HPP

#include "../packet.hpp"

namespace yb {

class device
{
public:
	virtual task<void> read_packet(packet & p) = 0;
	virtual task<void> write_packet(packet const & p) = 0;
};

} // namespace yb

#endif // LIBYB_ASYNC_DEVICE_HPP
