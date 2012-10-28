#include "descriptor_reader.hpp"
#include "channel.hpp"
#include "../utils/noncopyable.hpp"

namespace yb {

namespace {

struct handler
	: packet_handler, noncopyable
{
	explicit handler(device & d)
		: m_dev(d), m_registered(true)
	{
		m_reg = d.register_receiver(*this);
	}

	~handler()
	{
		if (m_registered)
			m_dev.unregister_receiver(m_reg);
	}

	void handle_packet(packet const & p)
	{
		if (p[0] != 0)
			return;

		try
		{
			m_buffer.insert(m_buffer.end(), p.begin() + 1, p.end());
			if (p.size() != 16)
			{
				device_descriptor dd = device_descriptor::parse(m_buffer.data(), m_buffer.data() + m_buffer.size());
				m_out.send(std::move(dd));

				m_dev.unregister_receiver(m_reg);
				m_registered = false;
			}
		}
		catch (...)
		{
			m_out.send(std::current_exception());
			m_dev.unregister_receiver(m_reg);
			m_registered = false;
		}
	}

	device & m_dev;

	std::vector<uint8_t> m_buffer;
	channel<device_descriptor> m_out;

	bool m_registered;
	device::receiver_registration m_reg;
};

}

task<device_descriptor> read_device_descriptor(device & d)
{
	try
	{
		std::shared_ptr<handler> h(new handler(d));
		d.write_packet(yb::make_packet(0) % 0);
		return h->m_out.receive().follow_with([h](device_descriptor const &) {});
	}
	catch (...)
	{
		return async::raise<device_descriptor>();
	}
}

} // namespace yb
