#include "stream_parser.hpp"
#include "utils/noncopyable.hpp"
#include <utility> // move
using namespace yb;

stream_parser::stream_parser()
	: m_packet_pos(0)
{
}

void stream_parser::parse(packet_handler & h, buffer_ref const & buffer)
{
	size_t r = buffer.size();

	for (size_t i = 0; i < r; )
	{
		switch (m_packet_pos)
		{
		case 0:
			while (buffer[i] != 0x80 && i < r)
				++i;
			if (i < r)
			{
				m_packet_pos = 1;
				++i;
			}
			break;

		case 1:
			m_partial_packet.resize((buffer[i] & 0xf) + 1);
			m_partial_packet[0] = buffer[i] >> 4;
			++i;
			++m_packet_pos;

			if (m_partial_packet.size() == 1)
			{
				h.handle_packet(std::move(m_partial_packet));
				m_partial_packet.clear();
				m_packet_pos = 0;
				break;
			}

			// fallthrough

		default:
			while (i < r)
			{
				m_partial_packet[m_packet_pos-1] = buffer[i];
				++i;

				if (m_packet_pos == m_partial_packet.size())
				{
					h.handle_packet(std::move(m_partial_packet));
					m_partial_packet.clear();
					m_packet_pos = 0;
					break;
				}

				++m_packet_pos;
			}
		}
	}
}

void stream_parser::parse(std::vector<packet> & out, buffer_ref const & buffer)
{
	class handler
		: public packet_handler, noncopyable
	{
	public:
		handler(std::vector<packet> & out)
			: m_out(out)
		{
		}

		void handle_packet(packet const & p)
		{
			m_out.push_back(p);
		}

	private:
		std::vector<packet> & m_out;
	};

	handler h(out);
	this->parse(h, buffer);
}
