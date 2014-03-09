#include "../console_stream.hpp"
#include "linux_fdpoll_task.hpp"
#include "unix_file.hpp"
#include <unistd.h>
using namespace yb;

struct console_stream::impl
{
	int fd;
};

console_stream::console_stream(output_kind_t output_kind)
	: m_pimpl(0)
{
	m_pimpl = new impl();
	if (output_kind == out)
		m_pimpl->fd = 1;
	else
		m_pimpl->fd = 2;
}

console_stream::~console_stream()
{
	delete m_pimpl;
}

task<buffer_view> console_stream::read(buffer_policy policy, size_t max_size)
{
	return detail::read_linux_fd(/*fd=*/0, std::move(policy), max_size);
}

task<size_t> console_stream::write(buffer_ref buf)
{
	return detail::write_linux_fd(m_pimpl->fd, buf);
}
