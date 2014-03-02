#include "../file.hpp"
#include "../../utils/detail/win32_file_operation.hpp"
#include <windows.h>

struct yb::file::impl
{
	HANDLE m_handle;
	LONG64 m_file_offset;
};

yb::file::file(yb::string_ref const & fname)
{
	std::unique_ptr<impl> pimpl(new impl());
	pimpl->m_file_offset = 0;
	pimpl->m_handle = CreateFileA(fname.to_string().c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
	if (pimpl->m_handle == INVALID_HANDLE_VALUE)
		throw std::runtime_error("failed to open file");

	m_pimpl = pimpl.release();
}

yb::file::~file()
{
	this->clear();
}

void yb::file::clear()
{
	if (m_pimpl)
	{
		CloseHandle(m_pimpl->m_handle);
		delete m_pimpl;
		m_pimpl = 0;
	}
}

yb::file::file_size_t yb::file::size() const
{
	LARGE_INTEGER li;
	if (!GetFileSizeEx(m_pimpl->m_handle, &li))
		throw std::runtime_error("failed to retrieve file size");

	return (file_size_t)li.QuadPart;
}

yb::task<yb::buffer_view> yb::file::read(buffer_policy policy, size_t max_size)
{
	struct ctx_t
	{
		yb::detail::win32_file_operation m_op;
		yb::buffer m_buffer;
	};

	std::shared_ptr<ctx_t> ctx = std::make_shared<ctx_t>();

	return policy.fetch(1, max_size).then([this, ctx, max_size](yb::buffer buf) {
		ctx->m_buffer = std::move(buf);
		return ctx->m_op.read(m_pimpl->m_handle, ctx->m_buffer.data(), (std::min)(ctx->m_buffer.size(), max_size), m_pimpl->m_file_offset).then([this, ctx](std::size_t r) {
			InterlockedAdd64(&m_pimpl->m_file_offset, r);
			return buffer_view(std::move(ctx->m_buffer), r);
		});
	});
}

yb::task<size_t> yb::file::write(buffer_ref buf)
{
	return yb::async::raise<size_t>(std::runtime_error("not implemented"));
}
