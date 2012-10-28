#ifndef LIBYB_TUNNEL_HPP
#define LIBYB_TUNNEL_HPP

#include "async/task.hpp"
#include "async/stream.hpp"
#include "async/device.hpp"
#include "async/promise.hpp"
#include "descriptor.hpp"
#include "utils/signal.hpp"
#include <deque>

namespace yb {

class tunnel_handler
	: private packet_handler
{
public:
	typedef std::vector<std::string> tunnel_list_t;

	tunnel_handler();
	~tunnel_handler();

	bool attach(device & dev, device_descriptor const & desc);

	void request_tunnel_list();

	template <typename F>
	void on_tunnel_list(F && f)
	{
		m_on_tunnel_list.add(std::forward<F>(f));
	}

	task<tunnel_list_t> list_tunnels();

	task<uint8_t> open(string_ref const & name);
	void fast_close(uint8_t pipe_no);
	task<size_t> read(uint8_t pipe_no, uint8_t * buffer, size_t size);
	task<size_t> write(uint8_t pipe_no, uint8_t const * buffer, size_t size);

private:
	void handle_packet(packet const & p);
	static tunnel_list_t parse_tunnel_list(packet const & p);

	std::list<promise<uint8_t>> m_active_opens;

	struct read_irp
	{
		uint8_t * buffer;
		size_t size;
		promise<size_t> transferred;
	};
	std::map<uint8_t, std::deque<read_irp>> m_read_irps;

	device * m_dev;
	device::receiver_registration m_reg;
	device_config m_config;

	signal<tunnel_list_t> m_on_tunnel_list;

	friend class tunnel_stream;
};

class tunnel_stream
	: public stream
{
public:
	tunnel_stream();
	~tunnel_stream();

	task<void> open(tunnel_handler & th, string_ref const & name);
	void fast_close();

	task<size_t> read(uint8_t * buffer, size_t size);
	task<size_t> write(uint8_t const * buffer, size_t size);

private:
	tunnel_handler * m_th;
	uint8_t m_pipe_no;
};

} // namespace yb

#endif // LIBYB_TUNNEL_HPP
