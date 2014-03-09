#include "../net.hpp"
#include "../../async/detail/unix_file.hpp"
#include "../../async/detail/linux_fdpoll_task.hpp"
#include "../../utils/detail/scoped_unix_fd.hpp"
#include "../../utils/detail/unix_system_error.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
using namespace yb;

struct tcp_socket::impl
{
	int fd;
};

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
	this->clear();
}

tcp_socket::tcp_socket(tcp_socket && o)
	: m_pimpl(o.m_pimpl)
{
	o.m_pimpl = 0;
}

tcp_socket & tcp_socket::operator=(tcp_socket && o)
{
	tcp_socket tmp(std::move(o));
	std::swap(m_pimpl, tmp.m_pimpl);
}

void tcp_socket::clear()
{
	if (m_pimpl)
	{
		::close(m_pimpl->fd);
		delete m_pimpl;
		m_pimpl = 0;
	}
}

task<buffer_view> tcp_socket::read(buffer_policy policy, size_t max_size)
{
	return detail::read_linux_fd(m_pimpl->fd, std::move(policy), max_size);
}

task<size_t> tcp_socket::write(buffer_ref buf)
{
	return detail::write_linux_fd(m_pimpl->fd, buf);
}

task<void> yb::tcp_listen(
	uint16_t port,
	std::function<void(tcp_socket &)> const & handler,
	ipv4_address const & intf,
	std::function<bool(tcp_incoming const &)> const & cond)
{
	struct ctx_t
	{
		detail::scoped_unix_fd s;
		std::function<void(tcp_socket &)> handler;
		std::function<bool(tcp_incoming const &)> cond;
	};

	std::shared_ptr<ctx_t> ctx(std::make_shared<ctx_t>());
	ctx->s.reset(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
	if (ctx->s.get() == -1)
		return async::raise<void>(detail::unix_system_error(errno));
	ctx->handler = handler;
	ctx->cond = cond;

	struct sockaddr_in sa = {};
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	memcpy(&sa.sin_addr, &intf, 4);

	if (::bind(ctx->s.get(), (sockaddr const *)&sa, sizeof sa) == -1)
		return async::raise<void>(detail::unix_system_error(errno));

	if (::listen(ctx->s.get(), SOMAXCONN) == -1)
		return async::raise<void>(detail::unix_system_error(errno));

	return yb::loop([ctx](cancel_level cl) -> task<void> {
		if (cl >= cl_quit)
			return yb::nulltask;

		return make_linux_pollfd_task(ctx->s.get(), POLLIN, [](cancel_level cl) {
			return cl < cl_quit;
		}).then([ctx](short revents) -> task<void> {
			struct sockaddr_in sa;
			socklen_t sa_len = sizeof sa;
			detail::scoped_unix_fd r(::accept(ctx->s.get(), (struct sockaddr *)&sa, &sa_len));
			if (r.get() == -1)
				return async::raise<void>(detail::unix_system_error(errno));

			if (sa_len != sizeof sa || sa.sin_family != AF_INET)
				return async::value();

			yb::tcp_incoming inc;
			memcpy(&inc.addr, &sa.sin_addr, 4);
			inc.port = ntohs(sa.sin_port);
			if (ctx->cond && !ctx->cond(inc))
				return async::value();

			std::unique_ptr<tcp_socket::impl> pimpl(new tcp_socket::impl());
			pimpl->fd = r.release();

			tcp_socket sock(pimpl.release());
			ctx->handler(sock);
			return async::value();
		});
	});
}
