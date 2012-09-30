#ifndef LIBYB_PACKET_HPP
#define LIBYB_PACKET_HPP

#include <stdint.h>
#include <vector>
#include "vector_ref.hpp"

namespace yb {

// The packet must always be at least one character long (the command).
// UART-based Shupito devices limit the length of the packet to 16-bytes
// (including the command byte) and the value of the first byte
// must be no greater than 15.
typedef std::vector<uint8_t> packet;

namespace detail {

class packet_builder
{
public:
	packet_builder(uint8_t cmd)
		: m_packet(1, cmd)
	{
	}

	packet_builder & operator%(uint8_t b)
	{
		m_packet.push_back(b);
		return *this;
	}

	packet_builder & operator%(string_ref const & s)
	{
		uint8_t const * p = reinterpret_cast<uint8_t const *>(s.data());
		m_packet.insert(m_packet.end(), p, p + s.size());
		return *this;
	}

	packet_builder & operator%(buffer_ref const & s)
	{
		m_packet.insert(m_packet.end(), s.data(), s.data() + s.size());
		return *this;
	}

	operator packet() const
	{
		return m_packet;
	}

private:
	packet m_packet;
};

}

inline detail::packet_builder make_packet(uint8_t cmd)
{
	return detail::packet_builder(cmd);
}

} // namespace yb

#endif // LIBYB_PACKET_HPP
