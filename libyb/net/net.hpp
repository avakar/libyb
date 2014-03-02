#ifndef LIBYB_NET_NET_HPP
#define LIBYB_NET_NET_HPP

#include "../vector_ref.hpp"
#include "../async/task.hpp"
#include "../async/stream.hpp"
#include <functional>

namespace yb {

class tcp_socket
	: public stream
{
public:
	struct impl;

	tcp_socket();
	explicit tcp_socket(impl * pimpl);
	~tcp_socket();
	tcp_socket(tcp_socket && o);
	tcp_socket & operator=(tcp_socket && o);

	void clear();

	task<buffer_view> read(buffer_policy policy, size_t max_size) override;
	task<size_t> write(buffer_ref buf) override;

private:
	impl * m_pimpl;

	tcp_socket(tcp_socket const &);
	tcp_socket & operator=(tcp_socket const &);
};

struct ipv4_address
{
	uint8_t elems[4];
};

task<tcp_socket> tcp_connect(ipv4_address const & addr, uint16_t port);
task<tcp_socket> tcp_connect(string_ref const & host, uint16_t port);

struct tcp_incoming
{
	ipv4_address addr;
	uint16_t port;
};

task<ipv4_address> resolve_host(string_ref const & host);

task<void> tcp_listen(
	uint16_t port,
	std::function<void(tcp_socket &)> const & handler,
	ipv4_address const & intf = ipv4_address(),
	std::function<bool(tcp_incoming const &)> const & cond = std::function<bool(tcp_incoming const &)>());

task<void> tcp_listen(
	uint16_t port,
	std::function<void(tcp_socket &)> const & handler,
	string_ref const & intf,
	std::function<bool(tcp_incoming const &)> const & cond = std::function<bool(tcp_incoming const &)>());

} // namespace yb

#endif // LIBYB_NET_NET_HPP
