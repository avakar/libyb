#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

#include "net.hpp"
#include "../utils/detail/scoped_win32_handle.hpp"
#include "../utils/detail/win32_overlapped.hpp"
#include "../async/detail/win32_handle_task.hpp"
#include <functional>
#include <utility>
#include <cassert>
using namespace yb;

namespace {

struct wsa_startup_guard
{
	wsa_startup_guard();
	~wsa_startup_guard();
};

void wsa_init()
{
	WSADATA wd;
	int r = WSAStartup(MAKEWORD(2, 2), &wd);
	if (r != 0)
		throw std::runtime_error("WSAStartup failed");
}

void wsa_cleanup()
{
	WSACleanup();
}

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
	wsa_init();
}

wsa_startup_guard::~wsa_startup_guard()
{
	wsa_cleanup();
}

tcp_socket::impl::impl()
	: m_socket(0)
{
	m_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED | 0x80/*WSA_FLAG_NO_HANDLE_INHERIT*/);
}

tcp_socket::impl::~impl()
{
	closesocket(m_socket);
}

tcp_socket::tcp_socket()
	: m_pimpl(0)
{
}

tcp_socket::tcp_socket(impl * pimpl)
	: m_pimpl(pimpl)
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

static task<ipv4_address> resolve_host_impl(string_ref const & host)
{
	addrinfo * res;
	int r = getaddrinfo(std::string(host).c_str(), 0, 0, &res);
	if (r != 0)
		return yb::async::raise<ipv4_address>(std::runtime_error("Failed to resolve address")); // XXX

	for (addrinfo * cur = res; cur; cur = cur->ai_next)
	{
		if (cur->ai_family != AF_INET)
			continue;

		ipv4_address rr;
		sockaddr_in const * inaddr = (sockaddr_in const *)cur->ai_addr;
		rr.elems[0] = inaddr->sin_addr.s_net;
		rr.elems[1] = inaddr->sin_addr.s_host;
		rr.elems[2] = inaddr->sin_addr.s_impno;
		rr.elems[3] = inaddr->sin_addr.s_lh;
		freeaddrinfo(res);

		return async::value(rr);
	}

	freeaddrinfo(res);
	return async::raise<ipv4_address>(std::runtime_error("Failed to resolve address")); // XXX
}

struct socket_holder
{
	SOCKET s;

	socket_holder(SOCKET s = INVALID_SOCKET)
		: s(s)
	{
	}

	socket_holder(socket_holder && s)
		: s(s.s)
	{
		s.s = INVALID_SOCKET;
	}

	~socket_holder()
	{
		if (s != INVALID_SOCKET)
			closesocket(s);
	}

	socket_holder & operator=(socket_holder && h)
	{
		if (s != INVALID_SOCKET)
			closesocket(s);
		s = h.s;
		h.s = INVALID_SOCKET;
		return *this;
	}
};

struct tcp_impl
{
	socket_holder s;
	detail::scoped_win32_handle ev;
	std::function<void(tcp_socket &)> handler;
	std::function<bool(tcp_incoming const &)> cond;
};

template <typename T>
static task<T> wsa_error()
{
	int dw = WSAGetLastError();
	return async::raise<T>(std::runtime_error("WSA error")); // XXX
}

static int CALLBACK tcp_accept_cond(
	IN LPWSABUF lpCallerId,
	IN LPWSABUF lpCallerData,
	IN OUT LPQOS lpSQOS,
	IN OUT LPQOS lpGQOS,
	IN LPWSABUF lpCalleeId,
	OUT LPWSABUF lpCalleeData,
	OUT GROUP FAR *g,
	IN DWORD_PTR dwCallbackData
	)
{
	assert(dwCallbackData);
	tcp_impl const * impl = (tcp_impl const *)dwCallbackData;
	assert(impl->cond);

	if (lpCallerId->len < sizeof(sockaddr_in))
		return CF_REJECT;

	sockaddr_in const * sa = (sockaddr_in const *)lpCallerId->buf;
	tcp_incoming inc;
	inc.addr.elems[0] = sa->sin_addr.s_net;
	inc.addr.elems[1] = sa->sin_addr.s_host;
	inc.addr.elems[2] = sa->sin_addr.s_impno;
	inc.addr.elems[3] = sa->sin_addr.s_lh;
	inc.port = ntohs(sa->sin_port);

	return impl->cond(inc)? CF_ACCEPT: CF_REJECT;
}

static task<void> tcp_listen_impl_loop(yb::cancel_level cl, std::shared_ptr<tcp_impl> const & impl)
{
	if (cl >= cl_quit)
		return yb::nulltask;

	return yb::make_win32_handle_task(impl->ev.get(), [](cancel_level cl) -> bool {
		return false;
	}).then([impl]() -> void {
		sockaddr_in sa;
		INT sa_len = sizeof sa;
		socket_holder client_socket;
		if (impl->cond)
			client_socket = WSAAccept(impl->s.s, (sockaddr *)&sa, &sa_len, &tcp_accept_cond, (DWORD_PTR)impl.get());
		else
			client_socket = WSAAccept(impl->s.s, (sockaddr *)&sa, &sa_len, 0, 0);

		if (client_socket.s == INVALID_SOCKET)
			return;

		tcp_socket::impl * pimpl = new tcp_socket::impl();
		pimpl->m_socket = client_socket.s;
		client_socket.s = INVALID_SOCKET;
		impl->handler(tcp_socket(pimpl));
	}).continue_with([](task<void> & r) -> task<void> {
		return async::value();
	});
}

static task<void> tcp_listen_impl(
	ipv4_address const & intf,
	uint16_t port,
	std::function<void(tcp_socket &)> const & handler,
	std::function<bool(tcp_incoming const &)> const & cond)
{
	std::shared_ptr<tcp_impl> impl = std::make_shared<tcp_impl>();
	impl->cond = cond;
	impl->handler = handler;

	impl->s = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);
	if (impl->s.s == INVALID_SOCKET)
		return wsa_error<void>();

	sockaddr_in sa = {};
	sa.sin_family = AF_INET;
	sa.sin_addr.s_net = intf.elems[0];
	sa.sin_addr.s_host = intf.elems[1];
	sa.sin_addr.s_impno = intf.elems[2];
	sa.sin_addr.s_lh = intf.elems[3];
	sa.sin_port = htons(port);

	if (bind(impl->s.s, (sockaddr const *)&sa, sizeof sa) != 0)
		return wsa_error<void>();

	impl->ev.reset(CreateEventW(0, FALSE, FALSE, 0));
	if (impl->ev.empty())
		return wsa_error<void>();
	if (WSAEventSelect(impl->s.s, impl->ev.get(), FD_ACCEPT) != 0)
		return wsa_error<void>();
	if (listen(impl->s.s, SOMAXCONN) != 0)
		return wsa_error<void>();

	return yb::loop([impl](cancel_level cl) -> task<void> {
		return tcp_listen_impl_loop(cl, impl);
	});
}

task<ipv4_address> yb::resolve_host(string_ref const & host)
{
	wsa_init();
	return resolve_host_impl(host).follow_with([](ipv4_address const &) {
		wsa_cleanup();
	});
}

task<void> yb::tcp_listen(
	uint16_t port,
	std::function<void(tcp_socket &)> const & handler,
	ipv4_address const & intf,
	std::function<bool(tcp_incoming const &)> const & cond)
{
	wsa_init();
	return tcp_listen_impl(intf, port, handler, cond).follow_with([]() {
		wsa_cleanup();
	});
}

task<void> yb::tcp_listen(
	uint16_t port,
	std::function<void(tcp_socket &)> const & handler,
	string_ref const & intf,
	std::function<bool(tcp_incoming const &)> const & cond)
{
	wsa_init();
	return resolve_host_impl(intf).then([port, handler, cond](ipv4_address const & addr) {
		return tcp_listen_impl(addr, port, handler, cond);
	}).follow_with([]() {
		wsa_cleanup();
	});
}

struct socket_op
{
	WSABUF buf;
	detail::win32_overlapped o;
};

task<size_t> yb::tcp_socket::read(uint8_t * buffer, size_t size)
{
	std::shared_ptr<socket_op> op = std::make_shared<socket_op>();

	op->buf.len = size;
	op->buf.buf = (CHAR *)buffer;

	DWORD dwReceived = 0;
	DWORD dwFlags = 0;
	if (WSARecv(m_pimpl->m_socket, &op->buf, 1, &dwReceived, &dwFlags, &op->o.o, 0) == 0)
		return yb::async::value((size_t)dwReceived);

	if (WSAGetLastError() != WSA_IO_PENDING)
		return wsa_error<size_t>();

	return yb::make_win32_handle_task(op->o.o.hEvent, [this, op](cancel_level cl) -> bool {
		detail::cancel_win32_overlapped((HANDLE)m_pimpl->m_socket, op->o);
		return true;
	}).then([this, op]() ->task<size_t> {
		DWORD dwReceived;
		DWORD dwFlags;
		WSAGetOverlappedResult(m_pimpl->m_socket, &op->o.o, &dwReceived, FALSE, &dwFlags);
		return yb::async::value((size_t)dwReceived);
	});
}

task<size_t> yb::tcp_socket::write(uint8_t const * buffer, size_t size)
{
	std::shared_ptr<socket_op> op = std::make_shared<socket_op>();

	op->buf.len = size;
	op->buf.buf = (CHAR *)buffer;

	DWORD dwReceived = 0;
	if (WSASend(m_pimpl->m_socket, &op->buf, 1, &dwReceived, 0, &op->o.o, 0) == 0)
		return yb::async::value((size_t)dwReceived);

	if (WSAGetLastError() != WSA_IO_PENDING)
		return wsa_error<size_t>();

	return yb::make_win32_handle_task(op->o.o.hEvent, [this, op](cancel_level cl) -> bool {
		detail::cancel_win32_overlapped((HANDLE)m_pimpl->m_socket, op->o);
		return true;
	}).then([this, op]() -> task<size_t> {
		DWORD dwSent;
		DWORD dwFlags;
		WSAGetOverlappedResult(m_pimpl->m_socket, &op->o.o, &dwSent, FALSE, &dwFlags);
		return yb::async::value((size_t)dwSent);
	});
}

struct tcp_connect_ctx
{
	detail::scoped_win32_handle ev;
	std::unique_ptr<tcp_socket::impl> pimpl;
};

static task<tcp_socket> tcp_connect_impl(ipv4_address const & addr, uint16_t port)
{
	std::shared_ptr<tcp_connect_ctx> ctx(std::make_shared<tcp_connect_ctx>());
	ctx->ev.reset(CreateEvent(0, FALSE, FALSE, 0));
	if (ctx->ev.empty())
		return async::raise<tcp_socket>(std::runtime_error("cannot create event")); // XXX

	ctx->pimpl.reset(new tcp_socket::impl());
	ctx->pimpl->m_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);
	if (ctx->pimpl->m_socket == INVALID_SOCKET)
		return wsa_error<tcp_socket>();

	if (WSAEventSelect(ctx->pimpl->m_socket, ctx->ev.get(), FD_CONNECT) != 0)
		return wsa_error<tcp_socket>();

	sockaddr_in sa = {};
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	sa.sin_addr.s_net = addr.elems[0];
	sa.sin_addr.s_host = addr.elems[1];
	sa.sin_addr.s_impno = addr.elems[2];
	sa.sin_addr.s_lh = addr.elems[3];

	int r = connect(ctx->pimpl->m_socket, (sockaddr const *)&sa, sizeof sa);
	if (r == 0)
	{
		WSAEventSelect(ctx->pimpl->m_socket, 0, 0);
		return async::value(tcp_socket(ctx->pimpl.release()));
	}

	if (WSAGetLastError() != WSAEWOULDBLOCK)
		return wsa_error<tcp_socket>();

	return yb::make_win32_handle_task(ctx->ev.get(), [](cancel_level cl) -> bool {
		return false;
	}).then([ctx, sa]() -> task<tcp_socket> {
		connect(ctx->pimpl->m_socket, (sockaddr const *)&sa, sizeof(sockaddr_in));
		return yb::async::value(tcp_socket(ctx->pimpl.release()));
	});
}

task<tcp_socket> yb::tcp_connect(ipv4_address const & addr, uint16_t port)
{
	wsa_init();
	return tcp_connect_impl(addr, port).follow_with([](tcp_socket const &) {
		wsa_cleanup();
	});
}

task<tcp_socket> yb::tcp_connect(string_ref const & host, uint16_t port)
{
	wsa_init();
	return resolve_host_impl(host).then([port](ipv4_address const & addr) {
		return tcp_connect_impl(addr, port);
	}).follow_with([](tcp_socket const &) {
		wsa_cleanup();
	});
}

void yb::tcp_socket::clear()
{
	delete m_pimpl;
	m_pimpl = 0;
}
