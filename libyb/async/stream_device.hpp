#ifndef LIBYB_ASYNC_STREAM_DEVICE_HPP
#define LIBYB_ASYNC_STREAM_DEVICE_HPP

#include "device.hpp"
#include "stream.hpp"
#include "../stream_parser.hpp"
#include "signal.hpp"
#include <deque>

namespace yb {

class stream_device
	: public device
{
public:
	task<void> run(stream & s);
	void write_packet(packet const & p);

private:
	uint8_t m_read_buffer[256];
	stream_parser m_parser;

	signal m_start_write;
	std::vector<uint8_t> m_write_buffer;
	std::vector<uint8_t> m_write_backlog;

	task<void> write_loop(stream & s);
};

} // namespace yb

#endif // LIBYB_ASYNC_STREAM_DEVICE_HPP
