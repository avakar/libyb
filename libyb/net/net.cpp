#include "net.hpp"
#include <utility>
#include <winsock2.h>
using namespace yb;

namespace {

struct wsa_startup_guard
{
	wsa_startup_guard();
	~wsa_startup_guard();
};

}

struct tcp_socket::impl
{
	impl();
	~impl();

	wsa_startup_guard m_wsa_guard;
	SOCKET m_socket;
};

wsa_startup_guard::wsa_startup_guard()
{
	WSADATA wd;
	int r = WSAStartup(MAKEWORD(2, 2), &wd);
	if (r != 0)
		throw std::runtime_error("WSAStartup failed");
}

wsa_startup_guard::~wsa_startup_guard()
{
	WSACleanup();
}

tcp_socket::impl::impl()
	: m_socket(0)
{
	m_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
}

tcp_socket::impl::~impl()
{
	closesocket(m_socket);
}

tcp_socket::tcp_socket()
	: m_pimpl(0)
{
}

tcp_socket::~tcp_socket()
{
	delete m_pimpl;
}

tcp_socket::tcp_socket(tcp_socket && o)
	: m_pimpl(o.m_pimpl)
{
	o.m_pimpl = 0;
}

tcp_socket & tcp_socket::operator=(tcp_socket && o)
{
	std::swap(m_pimpl, o.m_pimpl);
	return *this;
}

tcp_socket tcp_socket::connect(string_ref const & host, uint16_t port)
{
	tcp_socket res;
	res.m_pimpl = new impl();

	return std::move(res);
}
