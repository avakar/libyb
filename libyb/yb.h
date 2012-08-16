#ifndef LIBYB_YB_H
#define LIBYB_YB_H

#include <stdint.h>
#include <vector>

// The packet must always be at least one character long (the command).
// UART-based Shupito devices limit the length of the packet to 16-bytes
// (including the command byte) and the value of the first byte
// must be no greater than 15.
typedef std::vector<uint8_t> yb_packet;

namespace detail {

class yb_packet_builder
{
public:
	yb_packet_builder(uint8_t cmd)
		: m_packet(1, cmd)
	{
	}

	yb_packet_builder & operator%(uint8_t b)
	{
		m_packet.push_back(b);
		return *this;
	}

	operator yb_packet() const
	{
		return m_packet;
	}

private:
	yb_packet m_packet;
};

}

inline detail::yb_packet_builder make_yb_packet(uint8_t cmd)
{
	return detail::yb_packet_builder(cmd);
}

#endif // LIBYB_YB_H
