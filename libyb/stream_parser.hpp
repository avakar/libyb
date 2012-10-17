#ifndef LIBYB_STREAM_PARSER_HPP
#define LIBYB_STREAM_PARSER_HPP

#include "packet.hpp"

namespace yb {

class stream_parser
{
public:
	stream_parser();
	void parse(std::vector<packet> & out, buffer_ref const & buffer);

private:
	size_t m_packet_pos;
	packet m_partial_packet;
};

} // namespace yb

#endif // LIBYB_STREAM_PARSER_HPP
