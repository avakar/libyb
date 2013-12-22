#ifndef LIBYB_NET_NET_HPP
#define LIBYB_NET_NET_HPP

#include "../vector_ref.hpp"

namespace yb {

class tcp_socket
{
public:
	tcp_socket();
	~tcp_socket();
	tcp_socket(tcp_socket && o);
	tcp_socket & operator=(tcp_socket && o);

	static tcp_socket connect(string_ref const & host, uint16_t port);

private:
	struct impl;
	impl * m_pimpl;

	tcp_socket(tcp_socket const &);
	tcp_socket & operator=(tcp_socket const &);
};

} // namespace yb

#endif // LIBYB_NET_NET_HPP
